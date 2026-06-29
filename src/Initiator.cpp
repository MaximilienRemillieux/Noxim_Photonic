/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the implementation of the buffer
 */
#include "Hub.h"
#include "PhotonicHub.h"
#include "Initiator.h"

void Initiator::thread_process()
{

	tlm::tlm_generic_payload* trans = new tlm::tlm_generic_payload;
	tlm::tlm_phase phase;
	sc_time delay;

	while (1)
	{
		LOG << " *** waiting for transmissions" << endl;

		wait(start_request_event);

		tlm::tlm_command cmd = tlm::TLM_WRITE_COMMAND;
		flit_payload = buffer_tx.Front();
		hub->power.antennaBufferFront();

		int destHub;

		// hub relay management  ////////////////////////////////////////////////////////////////
		// if explicitly set in the header flit, trasmission target should reach a relay hub
		if (flit_payload.flit_type == FLIT_TYPE_HEAD)
		{
			if (flit_payload.hub_relay_node!=NOT_VALID) {
				current_hub_relay = flit_payload.hub_relay_node;
				LOG << "HUB RELAY: Flit " << flit_payload << " setting transmission hub relay " << current_hub_relay << " to reach destination " << endl;
			}
			else
				current_hub_relay = NOT_VALID;
		}

		if (current_hub_relay!=NOT_VALID)
		{
			flit_payload.hub_relay_node = current_hub_relay;
			destHub = tile2Hub(flit_payload.hub_relay_node);
		}
		else
		{
			destHub = tile2Hub(flit_payload.dst_id);
		}
		////////////////////////////////////////////////////////////////////////////////


		LOG << " *** Starting transmission of " << flit_payload << " to reach HUB_" << destHub <<  endl;

		trans->set_command(cmd);
		trans->set_address(static_cast<const uint64>(destHub));

		trans->set_data_ptr(reinterpret_cast<unsigned char*>(&flit_payload));
		trans->set_data_length(sizeof(Flit));
		trans->set_streaming_width(sizeof(Flit)); // = data_length to indicate no streaming
		trans->set_byte_enable_ptr(0); // 0 indicates unused
		trans->set_dmi_allowed(false); // Mandatory initial value
		trans->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE); // Mandatory initial value

		delay = sc_time(0, SC_PS);

		// Call b_transport to demonstrate the b/nb conversion by the simple_target_socket
		socket->b_transport( *trans, delay);

		hub->power.wirelessTx(hub->local_id,destHub,GlobalParams::flit_size);

		// Initiator obliged to check response status and delay
		if (!trans->is_response_error() )
		{
			buffer_tx.Pop();
			hub->power.antennaBufferPop();

			if (flit_payload.flit_type == FLIT_TYPE_HEAD)
				hub->transmission_in_progress.at(_channel_id) = true;

			if (flit_payload.flit_type == FLIT_TYPE_TAIL)
			{
				LOG << "*** [Ch"<< _channel_id <<"] tail flit sent " << flit_payload << ", releasing token" << endl;
				hub->flag[_channel_id]->write(RELEASE_CHANNEL);
				hub->transmission_in_progress.at(_channel_id) = false;
			}
		}
		else
		{
			LOG << " WARNING: incomplete transaction " << endl;
		}

		//check_transaction( *trans );

	}

}

void InitiatorPhotonic::thread_process()
{

	tlm::tlm_generic_payload* trans = new tlm::tlm_generic_payload;
	tlm::tlm_phase phase;
	sc_time delay;

	while(1)
	{
		LOG << " *** waiting for transmissions" << endl;

		wait(start_request_event);

		tlm::tlm_command cmd = tlm::TLM_WRITE_COMMAND;
		flit_payload = buffer_tx.Front();
		photonichub->power.photonicBufferFront();

		int destPhotonicHub = NOT_VALID;

		if (flit_payload.flit_type == FLIT_TYPE_HEAD)
		{
			if (flit_payload.hub_relay_node != NOT_VALID) {
				current_wavelength_relay = flit_payload.hub_relay_node;
				LOG << "Photonic HUB RELAY: Flit " << flit_payload << " setting transmission wavelength relay " << current_wavelength_relay << " to reach destination " << endl;
			}
			else
				current_wavelength_relay = NOT_VALID;
		}

		if (current_wavelength_relay != NOT_VALID && isTileConnectedToPhotonicHub(current_wavelength_relay))
		{
			flit_payload.hub_relay_node = current_wavelength_relay;
			destPhotonicHub = tile2PhotonicHub(flit_payload.hub_relay_node);
		}
		else if (isTileConnectedToPhotonicHub(flit_payload.dst_id))
		{
			destPhotonicHub = tile2PhotonicHub(flit_payload.dst_id);
		}

		if (destPhotonicHub == NOT_VALID)
		{
			LOG << "WARNING: destination tile " << flit_payload.dst_id << " is not connected to any Photonic Hub; skipping photonic send." << endl;
			wait(SC_ZERO_TIME);
			continue;
		}

		LOG << " *** Starting transmission of " << flit_payload << " to reach Photonic HUB_" << destPhotonicHub <<  endl;

		trans->set_command(cmd);
		trans->set_address(static_cast<const uint64>(destPhotonicHub));

		trans->set_data_ptr(reinterpret_cast<unsigned char*>(&flit_payload));
		trans->set_data_length(sizeof(Flit));
		trans->set_streaming_width(sizeof(Flit)); // = data_length to indicate no streaming
		trans->set_byte_enable_ptr(0); // 0 indicates unused
		trans->set_dmi_allowed(false); // Mandatory initial value
		trans->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE); // Mandatory initial value

		delay = sc_time(0, SC_FS);

		// Call b_transport_photonic to demonstrate the b/nb conversion by the simple_target_socket
		socket->b_transport( *trans, delay);

		photonichub->power.photonicTx(photonichub->local_id,destPhotonicHub,GlobalParams::flit_size);
	
		// Initiator obliged to check response status and delay
		if (!trans->is_response_error() )
		{
			buffer_tx.Pop();
			photonichub->power.photonicBufferPop();

			if (flit_payload.flit_type == FLIT_TYPE_HEAD)
				photonichub->transmission_in_progress[_wavelength_id] = true;
			if (flit_payload.flit_type == FLIT_TYPE_TAIL)
			{
				LOG << "*** [Ch"<< _wavelength_id <<"] tail flit sent " << flit_payload << ", releasing token" << endl;
				photonichub->transmission_in_progress[_wavelength_id] = false;
			}
		}
		else
		{
			LOG << " WARNING: incomplete transaction " << endl;
		}
	}
}