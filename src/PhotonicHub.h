/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the declaration of the tile
 */

#ifndef __NOXIMPHOTONICHUB_H__
#define __NOXIMPHOTONICHUB_H__

#include <map>
#include <systemc.h>
#include "DataStructs.h"
#include "Buffer.h"
#include "ReservationTable.h"

#include "Initiator.h"
#include "Target.h"
#include "TokenRing.h"
#include "Power.h"

using namespace std;

SC_MODULE(PhotonicHub)
{
    SC_HAS_PROCESS(PhotonicHub);
    
    // I/O Ports
    sc_in_clk clock; // The input clock for the tile
    sc_in <bool> reset; // The reset signal for the tile

    int local_id; // Unique ID
    int num_ports;
    vector<int> attachedNodes;
    vector<int> wavelengths;

    sc_in<Flit>* flit_rx;
    sc_in<bool>* req_rx;
    sc_out<bool>* ack_rx;
    sc_out<TBufferFullStatus>* buffer_full_status_rx;

    sc_out<Flit>* flit_tx;
    sc_out<bool>* req_tx;	   
    sc_in<bool>* ack_tx;	  
    sc_in<TBufferFullStatus>* buffer_full_status_tx;

    BufferBank* buffer_from_tile;   // Buffer for each port
    BufferBank* buffer_to_tile;     // Buffer for each port
    bool* current_level_rx;	// Current level for ABP
    bool* current_level_tx;	// Current level for ABP

    map<int, bool> transmission_in_progress;

    map<int, Initiator*> init;
    map<int, Target*> target;

    map<int, int> tile2port_mapping;
    map<int, int> tile2hub_mapping;

    int start_from_port; // Port from which to start the reservation cycle
    int * start_from_vc; // VC from which to start the reservation cycle for the specific port

    ReservationTable photonic_reservation_table;

    void updateRxPower();
    void updateTxPower();
    void antennaToTileProcess();
    void tileToAntennaProcess();

    int selectWavelength(int src_hub, int dst_hub) const;

    int route(Flit&);
    int tile2Port(int);

    void setFlitTransmissionCycles(int cycles,int ch_id) {flit_transmission_cycles[ch_id]=cycles;}

    // Power stats

    Power power;

    //SC_METHOD(tileToPhotonicProcess);
    //sensitive << reset;
    //sensitive << clock.pos();
//
    //SC_METHOD(photonicToTileProcess);
    //sensitive << reset;
    //sensitive << clock.pos();

    int photonic_communications_counter;

    // Constructor
    PhotonicHub(sc_module_name nm, int id) : sc_module(nm)
    {
        local_id = id;

        num_ports = GlobalParams::hub_configuration[local_id].attachedNodes.size();
        attachedNodes = GlobalParams::hub_configuration[local_id].attachedNodes;

        wavelengths = GlobalParams::hub_configuration[local_id].txChannels;

        photonic_reservation_table.setSize(128);

        start_from_vc = new int[num_ports];

        buffer_from_tile = new BufferBank[num_ports];
        buffer_to_tile = new BufferBank[num_ports];

        current_level_rx = new bool[num_ports];
        current_level_tx = new bool[num_ports];

        start_from_port = 0;
    }

    int getID() { return local_id;}

    private:
    map<int,int> flit_transmission_cycles;

    void rxPowerManager();
    void txPowerManager();

    int route(Flit&);
    int tile2Port(int);

    int selectWavelength(int src_hub, int dst_hub) const;

    void tileToPhotonicProcess();
    void photonicToTileProcess();
};

#endif
