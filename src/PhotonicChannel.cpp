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
#include "PhotonicChannel.h"
PhotonicChannel::PhotonicChannel(sc_module_name nm, int id)
: sc_module(nm),
  targ_socket("targ_socket"),
  init_socket("init_socket")
{

    local_id = id;

    targ_socket.register_b_transport(this, &PhotonicChannel::b_transport);
    targ_socket.register_get_direct_mem_ptr(this, &PhotonicChannel::get_direct_mem_ptr);
    targ_socket.register_transport_dbg(this, &PhotonicChannel::transport_dbg);

    init_socket.register_invalidate_direct_mem_ptr(this, &PhotonicChannel::invalidate_direct_mem_ptr);

    int dataRate = GlobalParams::channel_configuration[local_id].dataRate;

    int flit_delay_ps =
        1000 * GlobalParams::flit_size / dataRate;

    flit_transmission_cycles =
        ceil((double)flit_delay_ps / GlobalParams::clock_period_ps);

    cc_flit_transmission_delay_ps =
        flit_transmission_cycles * GlobalParams::clock_period_ps;

    LOG << "PhotonicChannel "
        << local_id
        << " data rate "
        << dataRate
        << " Gbps, transmission delay "
        << flit_delay_ps
        << " ps (" << flit_transmission_cycles
        << " cycles)"
        << endl;
}

int PhotonicChannel::getFlitTransmissionCycles()
{
    return flit_transmission_cycles;
}

void PhotonicChannel::b_transport( int id, tlm::tlm_generic_payload& trans, sc_time& delay )
{

    // the total transmission delay is due to TLM Initiator delay +
    // photonic channel delay
    delay += sc_time(this->cc_flit_transmission_delay_ps, SC_PS);

    assert (id < (int)targ_socket.size());

    // Forward path
    sc_dt::uint64 address = trans.get_address();
    sc_dt::uint64 masked_address;
    unsigned int target_nr = decode_address( address, masked_address);

    if (target_nr < init_socket.size())
    {
	// Modify address within transaction
	trans.set_address( masked_address );


	accountPhotonicRxPower();

	powerManager(target_nr,trans);

	// Realize the delay annotated onto the transport call
	wait(delay);

	// Forward transaction to appropriate target
	init_socket[target_nr]->b_transport(trans, delay);


	// Replace original address
	trans.set_address( address );
    }
}


void PhotonicChannel::accountPhotonicRxPower()
{
    for (unsigned int i = 0; i<hubs.size();i++)
    {
	if (!GlobalParams::use_powermanager) 
	    hubs[i]->power.wirelessDynamicRx();
	else
	if (!(hubs[i]->power.isSleeping()))
	    hubs[i]->power.wirelessDynamicRx();
    }
}


void PhotonicChannel::powerManager(unsigned int hub_dst_index, tlm::tlm_generic_payload& trans)
{
    if (!GlobalParams::use_powermanager) return;

    struct Flit* f = (struct Flit*)trans.get_data_ptr();

    if (f->flit_type==FLIT_TYPE_HEAD)
    {
	int sleep_cycles = flit_transmission_cycles * f->sequence_length;

	for (unsigned int i = 0; i<hubs.size();i++)
	{

	    if (i!=hub_dst_index) 
	    {
		hubs[i]->power.rxSleep(sleep_cycles);
		LOG << " HUB_"<<hubs_id[i]<<" rxSleep() invoked with " << sleep_cycles << " cycles " << endl;
	    }
	}
    }

}


  bool PhotonicChannel::get_direct_mem_ptr(int id,
                                  tlm::tlm_generic_payload& trans,
                                  tlm::tlm_dmi&  dmi_data)
  {
    sc_dt::uint64 masked_address;
    unsigned int target_nr = decode_address( trans.get_address(), masked_address );
    if (target_nr >= init_socket.size())
      return false;

    trans.set_address( masked_address );

    bool status = init_socket[target_nr]->get_direct_mem_ptr( trans, dmi_data );

    // Calculate DMI address of target in system address space
    dmi_data.set_start_address( compose_address( target_nr, dmi_data.get_start_address() ));
    dmi_data.set_end_address  ( compose_address( target_nr, dmi_data.get_end_address() ));

    return status;
  }

  unsigned int PhotonicChannel::transport_dbg(int id, tlm::tlm_generic_payload& trans)
  {
    sc_dt::uint64 masked_address;
    unsigned int target_nr = decode_address( trans.get_address(), masked_address );
    if (target_nr >= init_socket.size())
      return 0;
    trans.set_address( masked_address );

    // Forward debug transaction to appropriate target
    return init_socket[target_nr]->transport_dbg( trans );
  }

  void PhotonicChannel::invalidate_direct_mem_ptr(int id,
                                         sc_dt::uint64 start_range,
                                         sc_dt::uint64 end_range)
  {
    // Reconstruct address range in system memory map
    sc_dt::uint64 bw_start_range = compose_address( id, start_range );
    sc_dt::uint64 bw_end_range   = compose_address( id, end_range );

    // Propagate call backward to all initiators
    for (unsigned int i = 0; i < targ_socket.size(); i++)
      targ_socket[i]->invalidate_direct_mem_ptr(bw_start_range, bw_end_range);
  }

unsigned int PhotonicChannel::decode_address(
        sc_dt::uint64 address,
        sc_dt::uint64& masked_address)
{

    int target_nr = NOT_VALID;

    masked_address = address;

    for (unsigned int i = 0; i < hubs_id.size(); i++)
    {
        if (hubs_id[i] == (int)masked_address)
        {
            target_nr = i;
            break;
        }
    }

    assert(target_nr != NOT_VALID);

    return target_nr;
}

sc_dt::uint64 PhotonicChannel::compose_address(
        unsigned int target_nr,
        sc_dt::uint64 address)
{
    return address;
}

void PhotonicChannel::addPhotonicHub(PhotonicHub* h)
{

    hubs.push_back(h);
    hubs_id.push_back(h->getID());

}
