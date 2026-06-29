/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the declaration of the global params needed by Noxim
 * to forward configuration to every sub-block
 */
#include "PhotonicHub.h"

// Initialize static wavelength allocator (ORNoC)
PhotonicWavelengthAllocator* PhotonicHub::wavelength_allocator = NULL;

int PhotonicHub::tile2Port(int id)
{
	// TODO: check all [..] map access to replace with at()
	return tile2port_mapping.at(id);
}

int PhotonicHub::route(Flit& f)
{
	// check if it is a local delivery
	for (vector<int>::size_type i=0; i< GlobalParams::photonic_hub_configuration[local_id].attachedNodesPhotonic.size();i++)
	{
		// ...to a destination which is connected to the PhotonicHub
		if (GlobalParams::photonic_hub_configuration[local_id].attachedNodesPhotonic[i]==f.dst_id)
		{
			return tile2Port(f.dst_id);
		}
		// ...or to a relay which is locally connected to the PhotonicHub
		if (GlobalParams::photonic_hub_configuration[local_id].attachedNodesPhotonic[i]==f.hub_relay_node)
		{
			//My modification
			// The router has already computed a relay node for wireless forwarding.
			// Return the tile port for this relay node.
			return tile2Port(f.hub_relay_node);
			//{ --- Previous code ---
			//assert(GlobalParams::winoc_dst_hops>0);
			//return tile2Port(f.hub_relay_node);
			//}
			//My modification end
		}
	}
	return DIRECTION_WAVEGUIDE;

}

void PhotonicHub::rxPowerManager()
{
	// Check wheter accounting or not buffer to tile leakage
	// For each port, two poweroff condition should be checked:
	// - the buffer to tile is empty
	// - it has not been reserved

	// currently only supported without VC
	assert(GlobalParams::n_virtual_channels==1);

	for (int port=0;port<num_ports;port++)
	{
		if (!buffer_to_tile[port][DEFAULT_VC].IsEmpty() ||
			photonic2tile_reservation_table.isNotReserved(port))
			power.leakageBufferToTile();

		else
			buffer_to_tile_poweroff_cycles[port]++;
	}


	for (unsigned int i=0;i<rxPhotonicChannels.size();i++)
	{
		int ch_id = rxPhotonicChannels[i];

		if (!target[ch_id]->buffer_rx.IsEmpty())
		{
			power.leakagePhotonicBuffer();
		}
		else
			buffer_rx_sleep_cycles[ch_id]++;
	}

	// Check wheter accounting photonic RX buffer
	// check if there is at least one not empty photonic RX buffer
	// To be only applied if the current hub is in RADIO_EVENT_SLEEP_ON mode

	if (power.isSleeping())
		total_sleep_cycles++;

	else // not sleeping
	{
		power.leakagephotodetectorRx();
		power.biasingRx();
	}
}

void PhotonicHub::updateRxPower()
{
	if (GlobalParams::use_powermanager)
		rxPowerManager();
	else
	{
		power.wirelessSnooping();
		power.leakageTransceiverRx();
		power.biasingRx();

		for (unsigned int i=0;i<rxPhotonicChannels.size();i++)
			for (int vc=0;vc<GlobalParams::n_virtual_channels;vc++)
				power.leakagePhotonicBuffer();

		for (int i = 0; i < num_ports; i++)
			for (int vc=0;vc<GlobalParams::n_virtual_channels;vc++)
				power.leakageBufferToTile();
	}
}

void PhotonicHub::txPowerManager()
{
	for (unsigned int i=0;i<txPhotonicChannels.size();i++)
	{
		// check if not empty or reserved
		if (!init[i]->buffer_tx.IsEmpty() ||
			tile2photonic_reservation_table.isNotReserved(i) )
		{
			power.leakagePhotonicBuffer();
			// check the second condition for turning off analog tx
			if (power.isSleeping())
			{
				analogtxoff_cycles[i]++;
			}
			else
			{
				power.leakageModulatorTx();
				power.biasingTx();
			}
		}
		else
		{   // abtx is empty and not reserved - turn off
			// note that this also applies to analog tx and serializer
			abtxoff_cycles[i]++;
			analogtxoff_cycles[i]++;
			total_ttxoff_cycles++;
		}
	}
}

void PhotonicHub::updateTxPower()
{
	if (GlobalParams::use_powermanager)
		txPowerManager();
	else
	{
		for (unsigned int i=0;i<txPhotonicChannels.size();i++)
			for (int vc=0;vc<GlobalParams::n_virtual_channels;vc++)
				power.leakagePhotonicBuffer();

		power.leakageTransceiverTx();
		power.biasingTx();
	}

	// mandatory
	power.leakageLinkRouter2PhotonicHub();
	for (int i = 0; i < num_ports; i++)
		for (int vc=0;vc<GlobalParams::n_virtual_channels;vc++)
			power.leakageBufferFromTile();
}

void PhotonicHub::photonicToTileProcess()
{
	if (reset.read())
	{
		for (int i = 0; i < num_ports; i++)
		{
			req_tx[i]->write(0);
			current_level_tx[i] = 0;
		}
		return;
	}
	// IMPORTANT: do not move from here
	// The rxPowerManager must perform its checks before the flits are removed from buffers
	updateRxPower();

	/***********************************************************************************
      data flow from photonic(s) towards the tiles consist of 3 different steps:

      1) data received from a phtonic_channel stored (if possible) to a specific buffer_rx
      2) data found on a buffer_rx moved to a buffer_to_tile
      3) data found on a buffer_to_tile moved to signal_tx 

      From a implementation perspective, they are performed in 3-2-1 order, to simulate
      a kind of pipelined sequence
     ***********************************************************************************/

	//////////////////////////////////////////////////////////////////////////////////////
	// Moves a flit from buffer_to_tile to the appropriate signal_tx
	// No routing required: each port is associated to a prefixed tile
	for (int i = 0; i < num_ports; i++)
	{
		// TODO: check blocking channel (like the blocking single signal ?)
		for (int k = 0;k < GlobalParams::n_virtual_channels; k++)
		{
			int vc = (start_from_vc[i]+k)%(GlobalParams::n_virtual_channels);

			if (!buffer_to_tile[i][vc].IsEmpty())
			{
				Flit flit = buffer_to_tile[i][vc].Front();

				LOG << "Flit " << flit << " found on buffer_to_tile[" << i <<"][" << vc << "] " << endl;
				if (current_level_tx[i] == ack_tx[i].read() &&
					buffer_full_status_tx[i].read().mask[vc] == false)
				{
					LOG << "Flit " << flit << " moved from buffer_to_tile[" << i <<"][" << vc << "] to signal flit_tx["<<i<<"] " << endl;

					flit_tx[i].write(flit);
					current_level_tx[i] = 1 - current_level_tx[i];
					req_tx[i].write(current_level_tx[i]);

					buffer_to_tile[i][vc].Pop();
					power.bufferToTilePop();
					power.r2phLink();
					break; // port flit transmitted, skip remaining VCs
				}
				else
				{
					LOG << "Flit " << flit << " cannot move from buffer_to_tile[" << i <<"] [" << vc << "] to signal flit_tx["<<i<<"] " 
						<< "current_level_tx=" << current_level_tx[i] << " ack_tx=" << ack_tx[i].read() 
						<< " buffer_full_mask[" << vc << "]=" << buffer_full_status_tx[i].read().mask[vc] << endl;
				}
			}//if buffer not empty
		}
		start_from_vc[i] = (start_from_vc[i]+1)%GlobalParams::n_virtual_channels;
	}

	/////////////////////////////////////////////////////////////////////////////////
	// Move a flit from photonic buffer_rx to the appropriate buffer_to_tile.
	//
	// Two different phases:
	// 1) stores routing decision about the incoming flit (e.g., to which output port)
	// 2) Moves the flits removing from photonic buffer_rx

	for (unsigned int i = 0; i < rxPhotonicChannels.size(); i++)
	{
		int photonic_channel = rxPhotonicChannels[i];
		if (!(target[photonic_channel]->buffer_rx.IsEmpty())){
			LOG << "### PhotonicHub " << local_id << " found flit in photonic buffer_rx for photonic_channel " << photonic_channel << endl;
			Flit received_flit = target[photonic_channel]->buffer_rx.Front();
			power.photonicBufferFront();

			// Check photonic buffer_rx making appropriate reservations
			if (received_flit.flit_type==FLIT_TYPE_HEAD)
			{
				int dst_port;

				if (received_flit.hub_relay_node!=NOT_VALID)
					dst_port = tile2Port(received_flit.hub_relay_node);
				else
                    dst_port = tile2Port(received_flit.dst_id);
				
				TReservation r;
				r.input = photonic_channel;
				r.vc = received_flit.vc_id; // use flit's original VC for other routing

				LOG << " Checking reservation availability of output port " << dst_port << " by photonic_channel " << photonic_channel << " for flit " << received_flit << endl;

				int rt_status = photonic2tile_reservation_table.checkReservation(r,dst_port);
				//photonic2tile_reservation_table.print();
				if (rt_status == RT_AVAILABLE)
				{
					LOG << "Reserving output port " << dst_port << " by photonic_channel " << photonic_channel << " for flit " << received_flit << endl;
					photonic2tile_reservation_table.reserve(r, dst_port);

					// The number of commucation using the wireless network, accounting also
					// partial wired path
					photonic_communications_counter++;
					cout << "[PHOTO_RX_HEAD_RESERVED] \t"
						<< " Flit src=" << received_flit.src_id << " dst=" << received_flit.dst_id
						<< " | Ring " << received_flit.photonic_ring_id 
						<< " | Wavelength " << received_flit.wavelength_id
						<< " | PhotonicHub " << local_id
						<< " | Incoming from PhotonicChannel " << photonic_channel
						<< " | Reserved output port " << dst_port << endl;
				}
				else if (rt_status == RT_ALREADY_SAME)
				{
					LOG << " RT_ALREADY_SAME reserved direction " << dst_port << " for flit " << received_flit << endl;
				}
				else if (rt_status == RT_OUTVC_BUSY)
				{
					LOG << " RT_OUTVC_BUSY reservation direction " << dst_port << " for flit " << received_flit << endl;
				}
				//else assert(false); // no meaningful status heres
			}
		}
	}
	// forwarding
	for (unsigned int i = 0; i < rxPhotonicChannels.size(); i++)
	{
		int photonic_channel = rxPhotonicChannels[i];
		vector<pair<int,int> > reservations = photonic2tile_reservation_table.getReservations(photonic_channel);

		LOG << "### PhotonicHub " << local_id << " forwarding phase for photonic_channel " << photonic_channel << ": found " << reservations.size() << " reservations" << endl;

		if (reservations.size()!=0)
		{
			int rnd_idx = rand()%reservations.size();

			int port = reservations[rnd_idx].first;
			int vc = reservations[rnd_idx].second;

			if (!(target[photonic_channel]->buffer_rx.IsEmpty()))
			{
				Flit received_flit = target[photonic_channel]->buffer_rx.Front();
				power.photonicBufferFront();

				if ( !buffer_to_tile[port][vc].IsFull() )
				{
					target[photonic_channel]->buffer_rx.Pop();
					power.photonicBufferPop();
					received_flit.vc_id = vc;

					LOG << "*** [PhotonicCh" << photonic_channel << "] Moving flit  " << received_flit << " from buffer_rx to buffer_to_tile[" << port <<"][" << vc << "]" << endl;

					buffer_to_tile[port][vc].Push(received_flit);
					power.bufferToTilePush();

					if (received_flit.flit_type == FLIT_TYPE_HEAD) {
						cout << "[PHOTO_RX_DELIVERED]\t"
							<< "Flit src=" << received_flit.src_id << " dst=" << received_flit.dst_id
							<< " | Ring " << received_flit.photonic_ring_id 
							<< " | Wavelength " << received_flit.wavelength_id
							<< " | PhotonicHub " << local_id
							<< " | Moved to buffer_to_tile[" << port << "][" << vc << "]" << endl;
					}
					
					if (received_flit.flit_type == FLIT_TYPE_TAIL)
					{
						LOG << "Releasing reservation for output port " << port << ", flit " << received_flit << endl;
						TReservation r;
						r.input = photonic_channel;
						r.vc = vc;
						photonic2tile_reservation_table.release(r,port);
					}
				}
				else
					LOG << "Full buffer_to_tile[" << port <<"][" << vc << "]" << ", cannot store " << received_flit << endl;
			}
			else
			{
				// should be ok
				/*
                LOG << "WARNING: empty target["<<photonic_channel<<"] buffer_rx, but reservation still present, if correct, remove assertion below " << endl;
                assert(false);
                */
			}
		}
	}
}

void PhotonicHub::tileToPhotonicProcess()
{
	if (reset.read())
	{

		TBufferFullStatus bfs;
		for (int i = 0; i < num_ports; i++)
		{
			ack_rx[i]->write(0);
			buffer_full_status_rx[i].write(bfs);
			current_level_rx[i] = 0;
		}
		return;
	}

	int last_reserved = NOT_VALID;

	// used to store routing decisions
	int * r_from_tile[num_ports];
	for (int i=0;i<num_ports;i++)
		r_from_tile[i] = new int[GlobalParams::n_virtual_channels];

	// 1st phase: Reservation
	for (int j = 0; j < num_ports; j++)
	{
		int i = (start_from_port + j) % (num_ports);

		for (int k = 0;k < GlobalParams::n_virtual_channels; k++)
		{
			int vc = (start_from_vc[i]+k)%(GlobalParams::n_virtual_channels);

			if (!buffer_from_tile[i][vc].IsEmpty())
			{
				LOG << "Reservation: buffer_from_tile[" << i <<"][" << vc << "] not empty " << endl;
				Flit flit = buffer_from_tile[i][vc].Front();

				assert(flit.vc_id == vc);

				power.bufferFromTileFront();
				r_from_tile[i][vc] = route(flit);

				if (flit.flit_type == FLIT_TYPE_HEAD)
				{
				//My modification
				// Only perform photonic_channel reservation if routing result is DIRECTION_WAVEGUIDE.
				// If routing returns a local port, no photonic reservation needed.
				if (r_from_tile[i][vc]==DIRECTION_WAVEGUIDE)
				//My modification end
				{
					TReservation r;
					r.input = i;
					r.vc = vc;
					//My modification
					//assert(r_from_tile[i][vc]==DIRECTION_WAVEGUIDE); // routing should return DIRECTION_WAVEGUIDE for waveguide flits
					//My modification end
					int photonic_channel;

				int dstPhotonicHub = NOT_VALID;
				if (flit.hub_relay_node != NOT_VALID && isTileConnectedToPhotonicHub(flit.hub_relay_node)) {
					dstPhotonicHub = tile2PhotonicHub(flit.hub_relay_node);
				}
				else if (isTileConnectedToPhotonicHub(flit.dst_id)) {
					dstPhotonicHub = tile2PhotonicHub(flit.dst_id);
				}

				if (dstPhotonicHub == NOT_VALID) {
					LOG << "Skipping photonic reservation: neither destination " << flit.dst_id << " nor relay " << flit.hub_relay_node << " is connected to a photonic hub." << endl;
					continue;
				}

				photonic_channel = selectWavelength(local_id, dstPhotonicHub);
				//cout << "------PhotonicHub " << local_id << " select wavelength 'photonic_channel' " << photonic_channel << " to reach dst hub " << dstPhotonicHub << endl;

				if (photonic_channel == NOT_VALID) {
					LOG << "WARNING: selectWavelength returned NOT_VALID for src hub " << local_id << " dst hub " << dstPhotonicHub << endl;
					continue;
				}

					LOG << "Checking reservation availability of Channel " << photonic_channel << " by PhotonicHub port[" << i << "][" << vc << "] for flit " << flit << endl;

					int rt_status = tile2photonic_reservation_table.checkReservation(r,photonic_channel);

					if (rt_status == RT_AVAILABLE)
					{
						LOG << "Reservation of photonic_channel " << photonic_channel << " from PhotonicHub port["<< i << "]["<<vc<<"] by flit " << flit << endl;
						tile2photonic_reservation_table.reserve(r, photonic_channel);
					}
					else if (rt_status == RT_ALREADY_SAME)
					{
						LOG << "RT_ALREADY_SAME reserved photonic_channel " << photonic_channel << " for flit " << flit << endl;
					}
					else if (rt_status == RT_OUTVC_BUSY)
					{
						LOG << "RT_OUTVC_BUSY reservation for photonic_channel " << photonic_channel << " for flit " << flit << endl;
					}
					else if (rt_status == RT_ALREADY_OTHER_OUT)
					{
						LOG << "RT_ALREADY_OTHER_OUT a photonic_channel different from " << photonic_channel << " already reserved by Hub port["<< i << "]["<<vc<<"]" << endl;
					}
					else assert(false); // no meaningful status here
				}
			}
			}
		}
		start_from_vc[i] = (start_from_vc[i]+1)%GlobalParams::n_virtual_channels;
	} // for num_ports

	if (last_reserved!=NOT_VALID)
		start_from_port = (last_reserved+1)%num_ports;

	// 2nd phase: Forwarding
	for (int i = 0; i < num_ports; i++)
	{
		vector<pair<int,int> > reservations = tile2photonic_reservation_table.getReservations(i);

		if (reservations.size()!=0)
		{
			int rnd_idx = rand()%reservations.size();

			int o = reservations[rnd_idx].first;
			int vc = reservations[rnd_idx].second;

			if (!buffer_from_tile[i][vc].IsEmpty())
			{
				Flit flit = buffer_from_tile[i][vc].Front();
				// powerFront already accounted in 1st phase

				//assert(r_from_tile[i][vc] == DIRECTION_WAVEGUIDE);

				int photonic_channel = o;

				if (photonic_channel != NOT_RESERVED)
				{
					if (!(init[photonic_channel]->buffer_tx.IsFull()) )
					{
						// Set wavelength allocation (ORNoC) fields on the flit
						flit.wavelength_id = photonic_channel;
						if (GlobalParams::use_wavelength_allocator && wavelength_allocator != NULL) {
							int dst_hub = NOT_VALID;
							if (flit.hub_relay_node != NOT_VALID && isTileConnectedToPhotonicHub(flit.hub_relay_node)) {
								dst_hub = tile2PhotonicHub(flit.hub_relay_node);
							}
							else if (isTileConnectedToPhotonicHub(flit.dst_id)) {
								dst_hub = tile2PhotonicHub(flit.dst_id);
							}
							if (dst_hub != NOT_VALID) {
								flit.photonic_ring_id = wavelength_allocator->getRing(local_id, dst_hub);
							} else {
								flit.photonic_ring_id = NOT_VALID;
								LOG << "WARNING: cannot determine destination photonic hub for flit " << flit << " when setting ring id." << endl;
							}	
							buffer_from_tile[i][vc].Pop();
							power.bufferFromTilePop();
							init[photonic_channel]->buffer_tx.Push(flit);
							power.photonicBufferPush();
							init[photonic_channel]->start_request_event.notify();

							cout << "[PHOTO_TX_QUEUED] \t"
							<< "  Flit src=" << flit.src_id << " dst=" << flit.dst_id
							<< " | Ring " << flit.photonic_ring_id << " | Wavelength " << flit.wavelength_id
							<< " | PhotonicHub " << local_id << " => PhotonicHub " << dst_hub
							<< " | PhotonicChannel " << photonic_channel << " | Type " << flit.flit_type << endl;

							if (flit.flit_type == FLIT_TYPE_TAIL)
							{
								TReservation r;
								r.input = i;
								r.vc = vc;
								tile2photonic_reservation_table.release(r,photonic_channel);
							}

							LOG << "Flit " << flit << " moved from buffer_from_tile["<<i<<"]["<<vc<<"] to buffer_tx["<<photonic_channel<<"] " << endl;
						}
					else
					{
						LOG << "Buffer Full: Cannot move flit " << flit << " from buffer_from_tile["<<i<<"] to buffer_tx["<<photonic_channel<<"] " << endl;
						//init[photonic_channel]->buffer_tx.Print();
					}
				}
				else
				{
					LOG << "Forwarding: No photonic_channel reserved for input port [" << i << "][" << vc << "], flit " << flit << endl;
				}
			}

		}// for all the ports
	}
// My modification
	// 2nd phase bis: Forward local flits (destinations attached to this hub or local relays)
	// These flits have route() returning a port index (0..num_ports-1), not DIRECTION_WAVEGUIDE
	for (int i = 0; i < num_ports; i++)
	{
		for (int vc = 0; vc < GlobalParams::n_virtual_channels; vc++)
		{
			if (!buffer_from_tile[i][vc].IsEmpty())
			{
				Flit flit = buffer_from_tile[i][vc].Front();
				int output_port = r_from_tile[i][vc];
				
				// Check if routing decision was a local port (not DIRECTION_WAVEGUIDE)
				if (output_port >= 0 && output_port < num_ports)
				{
					LOG << "Local forwarding flit " << flit << " from port[" << i << "][" << vc 
					    << "] to buffer_to_tile[" << output_port << "]" << endl;
					
					if (!buffer_to_tile[output_port][vc].IsFull())
					{
						buffer_from_tile[i][vc].Pop();
						power.bufferFromTilePop();
						buffer_to_tile[output_port][vc].Push(flit);
						power.bufferToTilePush();
						
						LOG << "Local flit moved to buffer_to_tile[" << output_port << "][" << vc << "]" << endl;
					}
					else
					{
						LOG << "buffer_to_tile[" << output_port << "][" << vc << "] IsFull, cannot move flit" << endl;
					}
				}
			}
		}
	}
// My modification end

	for (int i = 0; i < num_ports; i++)
	{

		if (req_rx[i]->read() == 1 - current_level_rx[i])
		{
			Flit received_flit = flit_rx[i]->read();
			int vc = received_flit.vc_id;
			LOG << "Reading " << received_flit << " from signal flit_rx[" << i << "]" << endl;

			if (!buffer_from_tile[i][vc].IsFull())
			{
				LOG << "Storing " << received_flit << " on buffer_from_tile[" << i << "][" << vc << "]" << endl;

				buffer_from_tile[i][vc].Push(received_flit);
				power.bufferFromTilePush();

				current_level_rx[i] = 1 - current_level_rx[i];
			}
			else
			{
				LOG << "Buffer Full: Cannot store " << received_flit << " on buffer_from_tile[" << i << "][" << vc << "]" << endl;
				//buffer_from_tile[i][TODO_VC].Print();
			}
		}
		ack_rx[i]->write(current_level_rx[i]);
		// updates the mask of VCs to prevent incoming data on full buffers
		TBufferFullStatus bfs;
		for (int vc=0;vc<GlobalParams::n_virtual_channels;vc++)
			bfs.mask[vc] = buffer_from_tile[i][vc].IsFull();
		buffer_full_status_rx[i].write(bfs);
	}

	// IMPORTANT: do not move from here
	// The txPowerManager assumes that all flit buffer write have been done
	updateTxPower();
}

}

int PhotonicHub::selectWavelength(int src_hub, int dst_hub) const
{
    // If ORNoC wavelength allocator is enabled and available, use it
    if (GlobalParams::use_wavelength_allocator && wavelength_allocator != NULL) {
        int wl = wavelength_allocator->getWavelength(src_hub, dst_hub);
        if (wl >= 0) {
			cout << "PhotonicHub.cpp: Wavalength allocated: " << wl << endl;
            return wl;  // Use allocated wavelength
        }
    }
    
    // Fallback to original random channel selection
    vector<int> & first = GlobalParams::photonic_hub_configuration[src_hub].txPhotonicChannels;
    vector<int> & second = GlobalParams::photonic_hub_configuration[dst_hub].rxPhotonicChannels;

    vector<int> intersection;

    for(unsigned int i=0;i<first.size();i++)
        for(unsigned int j=0;j<second.size();j++)
            if(first[i]==second[j])
                intersection.push_back(first[i]);

    if(intersection.empty())
        return NOT_VALID;

    return intersection[rand()%intersection.size()];
}