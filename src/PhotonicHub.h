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
#include "PhotonicWavelengthAllocator.h"

using namespace std;

SC_MODULE(PhotonicHub)
{
    SC_HAS_PROCESS(PhotonicHub);
    
    // I/O Ports
    sc_in_clk clock; // The input clock for the tile
    sc_in <bool> reset; // The reset signal for the tile

    int local_id; // Unique ID
    int num_ports;
    vector<int> attachedNodesPhotonic;
    vector<int> txPhotonicChannels;
    vector<int> rxPhotonicChannels;
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

    map<int, InitiatorPhotonic*> init;
    map<int, TargetPhotonic*> target;

    map<int, int> tile2port_mapping;
    map<int, int> tile2hub_mapping;

    int start_from_port; // Port from which to start the reservation cycle
    int * start_from_vc; // VC from which to start the reservation cycle for the specific port

    ReservationTable photonic2tile_reservation_table;
    ReservationTable tile2photonic_reservation_table;

    void updateRxPower();
    void updateTxPower();
    void photonicToTileProcess();
    void tileToPhotonicProcess();

    int selectWavelength(int src_hub, int dst_hub) const;

    int route(Flit&);
    int tile2Port(int);

    void setFlitTransmissionCycles(int cycles,int ch_id) {flit_transmission_cycles[ch_id]=cycles;}

    // Power stats

    Power power;
    
    // Wavelength allocator (ORNoC) - shared across all PhotonicHub instances
    static PhotonicWavelengthAllocator* wavelength_allocator;

    int total_sleep_cycles;
    int total_ttxoff_cycles;
    map<int,int> buffer_rx_sleep_cycles; // Photonic buffer RX power off cycles
    map<int,int> abtxoff_cycles; // Photonic buffer TX power off cycles
    map<int,int> analogtxoff_cycles; // analog TX power off cycles
    map<int,int> buffer_to_tile_poweroff_cycles;

    int photonic_communications_counter;

    // Constructor
    PhotonicHub(sc_module_name nm, int id) : sc_module(nm)
    {
    if (GlobalParams::use_photonic)
	{
	    SC_METHOD(photonicToTileProcess);
	    sensitive << reset;
	    sensitive << clock.pos();

	    SC_METHOD(tileToPhotonicProcess);
	    sensitive << reset;
	    sensitive << clock.pos();

	}
        local_id = id;
        num_ports = GlobalParams::photonic_hub_configuration[local_id].attachedNodesPhotonic.size();
        attachedNodesPhotonic = GlobalParams::photonic_hub_configuration[local_id].attachedNodesPhotonic;
        wavelengths = GlobalParams::photonic_hub_configuration[local_id].wavelengths;
        rxPhotonicChannels = GlobalParams::photonic_hub_configuration[local_id].rxPhotonicChannels;
        txPhotonicChannels = GlobalParams::photonic_hub_configuration[local_id].txPhotonicChannels;

        photonic2tile_reservation_table.setSize(num_ports);
        #define STATIC_MAX_CHANNELS 100
        tile2photonic_reservation_table.setSize(STATIC_MAX_CHANNELS);

        flit_rx = new sc_in<Flit>[num_ports];
        req_rx = new sc_in<bool>[num_ports];
        ack_rx = new sc_out<bool>[num_ports];
        buffer_full_status_rx = new sc_out<TBufferFullStatus>[num_ports];

        flit_tx = new sc_out<Flit>[num_ports];
        req_tx = new sc_out<bool>[num_ports];
        ack_tx = new sc_in<bool>[num_ports];
        buffer_full_status_tx = new sc_in<TBufferFullStatus>[num_ports];

        buffer_from_tile = new BufferBank[num_ports];
        buffer_to_tile = new BufferBank[num_ports];

        start_from_vc = new int[num_ports];

        current_level_rx = new bool[num_ports];
        current_level_tx = new bool[num_ports];

        start_from_port = 0;

        for(int i = 0; i < num_ports; i++) {
            for (int vc = 0;vc<GlobalParams::n_virtual_channels; vc++)
            {
                buffer_from_tile[i][vc].SetMaxBufferSize(GlobalParams::photonic_hub_configuration[local_id].fromTileBufferSizePhotonic);
                buffer_to_tile[i][vc].SetMaxBufferSize(GlobalParams::photonic_hub_configuration[local_id].toTileBufferSizePhotonic);
                buffer_from_tile[i][vc].setLabel(string(name())+"->bft["+i_to_string(i)+"]["+i_to_string(vc)+"]");
                buffer_to_tile[i][vc].setLabel(string(name())+"->btt["+i_to_string(i)+"]["+i_to_string(vc)+"]");
            }
            start_from_vc[i] = 0;
        }

        for (unsigned int i = 0; i < txPhotonicChannels.size(); i++) {
            char txt[50];
            int ch = txPhotonicChannels[i];
            sprintf(txt, "init_%d", ch);
            init[ch] = new InitiatorPhotonic(txt,this);
            init[ch]->buffer_tx.SetMaxBufferSize(GlobalParams::photonic_hub_configuration[local_id].txBufferSizePhotonic);
            init[ch]->buffer_tx.setLabel(string(name())+"->abtx["+i_to_string(i)+"]");
        }

        for (unsigned int i = 0; i < rxPhotonicChannels.size(); i++) {
            char txt[50];
            sprintf(txt, "target_%d", rxPhotonicChannels[i]);
            target[rxPhotonicChannels[i]] = new TargetPhotonic(txt, rxPhotonicChannels[i], this);
            target[rxPhotonicChannels[i]]->buffer_rx.SetMaxBufferSize(GlobalParams::photonic_hub_configuration[local_id].rxBufferSizePhotonic);
            target[rxPhotonicChannels[i]]->buffer_rx.setLabel(string(name())+"->abrx["+i_to_string(i)+"]");
        }

    start_from_port = 0;
	total_sleep_cycles = 0;
	total_ttxoff_cycles = 0;
	photonic_communications_counter = 0;

    // Initialize wavelength allocator (ORNoC)
    if (GlobalParams::use_wavelength_allocator) {
        if (wavelength_allocator == NULL) {
            wavelength_allocator = new PhotonicWavelengthAllocator(
                GlobalParams::photonic_hub_configuration.size(),
                GlobalParams::max_photonic_wavelengths);
            wavelength_allocator->generateORNoC(GlobalParams::max_photonic_wavelengths);
        }
    }
    }

    int getID() { return local_id;}

    private:
    map<int,int> flit_transmission_cycles;

    void rxPowerManager();
    void txPowerManager();

};

#endif
