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

int PhotonicHub::tile2Port(int id)
{
	// TODO: check all [..] map access to replace with at()
	return tile2port_mapping.at(id);
}

int PhotonicHub::route(Flit& f)
{
	// check if it is a local delivery
	for (vector<int>::size_type i=0; i< GlobalParams::hub_configuration[local_id].attachedNodes.size();i++)
	{
		// ...to a destination which is connected to the PhotonicHub
		if (GlobalParams::hub_configuration[local_id].attachedNodes[i]==f.dst_id)
		{
			return tile2Port(f.dst_id);
		}
		// ...or to a relay which is locally connected to the PhotonicHub
		if (GlobalParams::hub_configuration[local_id].attachedNodes[i]==f.hub_relay_node)
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
	return DIRECTION_WIRELESS;

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

    for(auto &it : target)
    {
        int wavelength = it.first;

        if(!target[wavelength]->buffer_rx.IsEmpty())
        {
            Flit flit = target[wavelength]->buffer_rx.Front();

            int port = tile2Port(flit.dst_id);

            if(!buffer_to_tile[port][flit.vc_id].IsFull())
            {
                target[wavelength]->buffer_rx.Pop();
                buffer_to_tile[port][flit.vc_id].Push(flit);
            }
        }
    }
}
void PhotonicHub::tileToPhotonicProcess()
{
    for(int i=0;i<num_ports;i++)
    {
        for(int vc=0;vc<GlobalParams::n_virtual_channels;vc++)
        {
            if(!buffer_from_tile[i][vc].IsEmpty())
            {
                Flit flit = buffer_from_tile[i][vc].Front();

                int dst_hub = tile2hub_mapping[flit.dst_id];

                int wavelength = selectWavelength(local_id,dst_hub);

                if(wavelength != NOT_VALID)
                {
                    buffer_from_tile[i][vc].Pop();
                    init[wavelength]->buffer_tx.Push(flit);
                }
            }
        }
    }

	// IMPORTANT: do not move from here
	// The txPowerManager assumes that all flit buffer write have been done
}

int PhotonicHub::selectWavelength(int src_hub, int dst_hub) const
{
    vector<int> & first = GlobalParams::hub_configuration[src_hub].txChannels;
    vector<int> & second = GlobalParams::hub_configuration[dst_hub].rxChannels;

    vector<int> intersection;

    for(unsigned int i=0;i<first.size();i++)
        for(unsigned int j=0;j<second.size();j++)
            if(first[i]==second[j])
                intersection.push_back(first[i]);

    if(intersection.empty())
        return NOT_VALID;

    return intersection[rand()%intersection.size()];
}