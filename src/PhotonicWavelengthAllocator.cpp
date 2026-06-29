/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file implements the wavelength allocation algorithm for photonic NoCs
 * based on the ORNoC (Optical Ring Network-on-Chip) methodology
 */

#include "PhotonicWavelengthAllocator.h"
#include <iostream>
#include <algorithm>
#include <iomanip>

using namespace std;

// ==================================================================================
// Static Ring Sequence Definition
// ==================================================================================
// Physical photonic ring topology for 4x4 grid of clusters
// Layout:
//   0   1   2   3
//   4   5   6   7
//   8   9  10  11
//  12  13  14  15
//
// Ring sequence (unidirectional, avoiding waveguide crossings):
// 0 -> 1 -> 2 -> 3 -> 7 -> 6 -> 5 -> 9 -> 10 -> 11 -> 15 -> 14 -> 13 -> 12 -> 8 -> 4 -> 0
const vector<int> PhotonicWavelengthAllocator::physical_ring_sequence = {
    0, 1, 2, 3, 7, 6, 5, 9, 10, 11, 15, 14, 13, 12, 8, 4
};

// ==================================================================================
// Constructor and Destructor
// ==================================================================================

PhotonicWavelengthAllocator::PhotonicWavelengthAllocator(int num_hubs, int max_wl)
    : num_hubs(num_hubs), max_wavelengths_per_ring(max_wl)
{
    // Validate inputs
    if (num_hubs <= 0) {
        cerr << "ERROR: PhotonicWavelengthAllocator - Invalid number of hubs: " << num_hubs << endl;
        return;
    }
    if (max_wl <= 0) {
        cerr << "WARNING: PhotonicWavelengthAllocator - Max wavelengths <= 0, forcing to 1" << endl;
        max_wavelengths_per_ring = 1;
    }
    
    // Initialize physical ring topology for path computation
    initializeRingPositionMap();
    
    // Initialize connectivity matrix
    populateConnectivity();
}

PhotonicWavelengthAllocator::~PhotonicWavelengthAllocator()
{
    reset();
}

// ==================================================================================
// Core ORNoC Wavelength Allocation Algorithm
// ==================================================================================

void PhotonicWavelengthAllocator::generateORNoC(int max_wavelengths)
{
    if (max_wavelengths <= 0) {
        max_wavelengths = 1;
    }
    max_wavelengths_per_ring = max_wavelengths;

    // Reset any previous allocation state before rebuilding the single-ring topology.
    communications.clear();
    allocations.clear();
    rings.clear();

    // Build communication matrix from the photonic hub configuration.
    cout << "ORNoC: Building communication matrix..." << endl;

    for (map<pair<int,int>, bool>::iterator it = connectivity_matrix.begin();
         it != connectivity_matrix.end(); ++it) {
        if (it->second) {
            pair<int,int> comm = it->first;
            communications[comm] = ORNoC_Communication(comm.first, comm.second);
            communications[comm].connectivity = true;
            cout << " PhotonicWavelengthAllocator.cpp: Communication (" << comm.first << " -> " << comm.second << ") is connected" << endl;
        }
    }

    cout << "ORNoC: Found " << communications.size() << " potential communications" << endl;

    // The architecture is fixed to one physical ring: Ring 0.
    // We create it once and always reuse it for every possible communication.
    PhotonicRing ring0(0, max_wavelengths_per_ring, true);
    rings.push_back(ring0);

    if (communications.empty()) {
        cout << "ORNoC: WARNING - No communications to allocate!" << endl;
    }

    // Allocate communications on Ring 0 only.
    // A communication is processed only if a wavelength is available on every
    // traversed portion of the physical ring path.
    for (map<pair<int,int>, ORNoC_Communication>::iterator it = communications.begin();
         it != communications.end(); ++it) {
        ORNoC_Communication& comm = it->second;

        if (comm.processed) {
            continue;
        }

        vector<pair<int,int>> portions = getPortions(comm.src, comm.dst);
        int wl = findAvailableWavelength(0, portions);

        if (wl >= 0 && reservePathWavelength(0, portions, wl)) {
            comm.ring = 0;
            comm.wavelength = wl;
            comm.path = portions;
            comm.processed = true;
            allocations[make_pair(comm.src, comm.dst)] = wl;
        }
    }

    // Final statistics
    cout << "ORNoC generation complete:" << endl;
    cout << "  - Total rings created: " << rings.size() << endl;
    cout << "  - Total allocations: " << allocations.size() << endl;
    cout << "  - Communications processed: ";
    int processed_count = 0;
    for (map<pair<int,int>, ORNoC_Communication>::iterator it = communications.begin();
         it != communications.end(); ++it) {
        if (it->second.processed) processed_count++;
    }
    cout << processed_count << " / " << communications.size() << endl;
}

// ==================================================================================
// Wavelength/Ring Query Functions
// ==================================================================================

int PhotonicWavelengthAllocator::getWavelength(int src_hub, int dst_hub) const
{
    pair<int,int> key = canonicalizeCommunication(src_hub, dst_hub);
    
    // Use safe map access to avoid exceptions
    map<pair<int,int>, int>::const_iterator it = allocations.find(key);
    if (it != allocations.end()) {
        return it->second;
    }
    
    return -1;  // NOT_VALID
}

int PhotonicWavelengthAllocator::getRing(int src_hub, int dst_hub) const
{
    pair<int,int> key = canonicalizeCommunication(src_hub, dst_hub);
    
    map<pair<int,int>, ORNoC_Communication>::const_iterator it = communications.find(key);
    if (it != communications.end()) {
        return it->second.ring;
    }
    
    return -1;  // NOT_VALID
}

// ==================================================================================
// Wavelength Availability and Reservation
// ==================================================================================

bool PhotonicWavelengthAllocator::isWavelengthAvailable(int ring_id, int src_hub, int dst_hub, int wavelength) const
{
    // Validate ring_id
    if (ring_id < 0 || ring_id >= (int)rings.size()) {
        return false;
    }
    
    // Validate wavelength
    if (wavelength < 0 || wavelength >= rings[ring_id].max_wavelengths) {
        return false;
    }
    
    // Check occupation map
    pair<int,int> portion = make_pair(src_hub, dst_hub);
    map<pair<int,int>, vector<bool>>::const_iterator it = rings[ring_id].occupation.find(portion);
    
    if (it != rings[ring_id].occupation.end()) {
        // Portion exists, check wavelength availability
        if (wavelength < (int)it->second.size()) {
            return !it->second[wavelength];  // Available if not occupied
        }
    }
    
    // Portion not tracked or wavelength index out of bounds = available (not yet used)
    return true;
}

bool PhotonicWavelengthAllocator::reserveWavelength(int ring_id, int src_hub, int dst_hub, int wavelength)
{
    // Validate ring_id
    if (ring_id < 0 || ring_id >= (int)rings.size()) {
        return false;
    }
    
    // Validate wavelength
    if (wavelength < 0 || wavelength >= rings[ring_id].max_wavelengths) {
        return false;
    }
    
    pair<int,int> portion = make_pair(src_hub, dst_hub);
    
    // Ensure occupation vector exists for this portion
    if (rings[ring_id].occupation.find(portion) == rings[ring_id].occupation.end()) {
        rings[ring_id].occupation[portion] = vector<bool>(rings[ring_id].max_wavelengths, false);
    }
    
    // Check if available
    if (isWavelengthAvailable(ring_id, src_hub, dst_hub, wavelength)) {
        rings[ring_id].occupation[portion][wavelength] = true;
        return true;
    }
    
    return false;
}

// ==================================================================================
// Protected Helper Functions
// ==================================================================================

void PhotonicWavelengthAllocator::populateConnectivity()
{
    // Build connectivity matrix from GlobalParams photonic hub configuration
    
    // First pass: mark all hubs as potentially connected
    map<int, PhotonicHubConfig>::const_iterator hub_it;
    
    for (hub_it = GlobalParams::photonic_hub_configuration.begin();
         hub_it != GlobalParams::photonic_hub_configuration.end(); ++hub_it) {
        
        int hub_id = hub_it->first;
        const PhotonicHubConfig& config = hub_it->second;
        
        // For each outgoing channel, this hub can send to connected hubs
        // (In a real network, this would be determined by network topology)
        for (size_t i = 0; i < config.txPhotonicChannels.size(); ++i) {
            for (size_t j = 0; j < config.rxPhotonicChannels.size(); ++j) {
                if (config.txPhotonicChannels[i] == config.rxPhotonicChannels[j]) {
                    // Same channel = can communicate with itself (or within local photonic ring)
                    // For now, mark as connected to all other hubs in config
                    map<int, PhotonicHubConfig>::const_iterator other_hub_it;
                    for (other_hub_it = GlobalParams::photonic_hub_configuration.begin();
                         other_hub_it != GlobalParams::photonic_hub_configuration.end(); ++other_hub_it) {
                        
                        int other_hub_id = other_hub_it->first;
                        if (other_hub_id != hub_id) {
                            pair<int,int> conn = canonicalizeCommunication(hub_id, other_hub_id);
                            connectivity_matrix[conn] = true;
                        }
                    }
                }
            }
        }
    }
    
    cout << "ORNoC: Connectivity matrix populated with " << connectivity_matrix.size() 
         << " connections" << endl;
}

void PhotonicWavelengthAllocator::initializeRingPositionMap()
{
    // Clear any existing mappings
    ring_position.clear();
    
    // Build mapping from hub_id to position in physical ring sequence
    for (size_t i = 0; i < physical_ring_sequence.size(); ++i) {
        int hub_id = physical_ring_sequence[i];
        ring_position[hub_id] = i;
    }
    
    cout << "ORNoC: Physical ring position map initialized:" << endl;
    cout << "  Ring sequence: ";
    for (size_t i = 0; i < physical_ring_sequence.size(); ++i) {
        cout << physical_ring_sequence[i];
        if (i < physical_ring_sequence.size() - 1) cout << " -> ";
    }
    cout << endl;
}

vector<pair<int,int>> PhotonicWavelengthAllocator::computePhysicalRingPath(int src, int dst) const
{
    vector<pair<int,int>> portions;
    
    // Validate that source and destination are in the ring
    if (ring_position.find(src) == ring_position.end() ||
        ring_position.find(dst) == ring_position.end()) {
        cerr << "ERROR: Hub " << (ring_position.find(src) == ring_position.end() ? src : dst) 
             << " not found in physical ring sequence" << endl;
        // Fallback: return direct connection
        portions.push_back(make_pair(src, dst));
        return portions;
    }
    
    // Get positions in the ring
    int pos_src = ring_position.at(src);
    int pos_dst = ring_position.at(dst);
    int ring_size = physical_ring_sequence.size();
    
    // Generate all portions following the ring unidirectionally
    // Ring is traversed in the order defined by physical_ring_sequence
    int current_pos = pos_src;
    
    while (current_pos != pos_dst) {
        int current_hub = physical_ring_sequence[current_pos];
        int next_pos = (current_pos + 1) % ring_size;
        int next_hub = physical_ring_sequence[next_pos];
        
        // Add portion from current to next
        portions.push_back(make_pair(current_hub, next_hub));
        
        current_pos = next_pos;
    }
    
    return portions;
}

vector<pair<int,int>> PhotonicWavelengthAllocator::getPortions(int src, int dst) const
{
    // Use physical ring topology to compute path
    return computePhysicalRingPath(src, dst);
}

int PhotonicWavelengthAllocator::findAvailableWavelength(int ring_id, const vector<pair<int,int>>& portions) const
{
    if (ring_id < 0 || ring_id >= (int)rings.size()) {
        return -1;
    }
    
    // Try each wavelength
    for (int wl = 0; wl < rings[ring_id].max_wavelengths; ++wl) {
        bool available_on_all_portions = true;
        
        // Check if this wavelength is available on ALL portions of the path
        for (size_t p = 0; p < portions.size(); ++p) {
            if (!isWavelengthAvailable(ring_id, portions[p].first, portions[p].second, wl)) {
                available_on_all_portions = false;
                break;
            }
        }
        
        if (available_on_all_portions) {
            //cout << "ORNoC: Found available wavelength " << wl << " on Ring " << ring_id 
            //     << " for portions: ";
            //for (const auto& portion : portions) {
            //    cout << "(" << portion.first << " -> " << portion.second << ") ";
            //}
            //cout << endl;
            return wl;  // Found available wavelength
        }
    }
    
    return -1;  // No wavelength available on this ring
}

bool PhotonicWavelengthAllocator::reservePathWavelength(int ring_id, const vector<pair<int,int>>& portions, int wavelength)
{
    // First, verify all portions are available
    int wl = findAvailableWavelength(ring_id, portions);
    if (wl != wavelength && findAvailableWavelength(ring_id, portions) >= 0) {
        // Try to find the requested wavelength specifically
        bool all_available = true;
        for (size_t p = 0; p < portions.size(); ++p) {
            if (!isWavelengthAvailable(ring_id, portions[p].first, portions[p].second, wavelength)) {
                all_available = false;
                break;
            }
        }
        if (!all_available) return false;
    }
    
    // Reserve on all portions
    for (size_t p = 0; p < portions.size(); ++p) {
        if (!reserveWavelength(ring_id, portions[p].first, portions[p].second, wavelength)) {
            // Rollback: unreserve already-reserved portions
            for (size_t rb = 0; rb < p; ++rb) {
                if (ring_id < (int)rings.size()) {
                    pair<int,int> portion = portions[rb];
                    if (rings[ring_id].occupation.find(portion) != rings[ring_id].occupation.end()) {
                        if (wavelength < (int)rings[ring_id].occupation[portion].size()) {
                            rings[ring_id].occupation[portion][wavelength] = false;
                        }
                    }
                }
            }
            return false;
        }
    }
    
    return true;
}

// ==================================================================================
// Utility Functions
// ==================================================================================

void PhotonicWavelengthAllocator::reset()
{
    communications.clear();
    allocations.clear();
    rings.clear();
    connectivity_matrix.clear();
}

void PhotonicWavelengthAllocator::printStatistics() const
{
    cout << "\n=== ORNoC Wavelength Allocation Statistics ===" << endl;
    cout << "Total hubs: " << num_hubs << endl;
    cout << "Max wavelengths per ring: " << max_wavelengths_per_ring << endl;
    cout << "Number of rings created: " << rings.size() << endl;
    cout << "Number of allocations: " << allocations.size() << endl;
    
    int processed_count = 0;
    int unprocessed_count = 0;
    for (map<pair<int,int>, ORNoC_Communication>::const_iterator it = communications.begin();
         it != communications.end(); ++it) {
        if (it->second.processed) {
            processed_count++;
        } else {
            unprocessed_count++;
        }
    }
    
    cout << "Communications processed: " << processed_count << endl;
    cout << "Communications unprocessed: " << unprocessed_count << endl;
    
    // Detailed ring information
    cout << "\nRing Details:" << endl;
    for (size_t r = 0; r < rings.size(); ++r) {
        cout << "  Ring " << r << ": ";
        cout << (rings[r].clockwise ? "Clockwise, " : "Counter-clockwise, ");
        cout << "Wavelengths: " << rings[r].max_wavelengths << ", ";
        
        int total_reserved = 0;
        for (map<pair<int,int>, vector<bool>>::const_iterator it = rings[r].occupation.begin();
             it != rings[r].occupation.end(); ++it) {
            for (size_t w = 0; w < it->second.size(); ++w) {
                if (it->second[w]) total_reserved++;
            }
        }
        cout << "Reserved wavelengths: " << total_reserved << endl;
    }
    
    // Allocations mapping
    cout << "\nWavelength Allocations:" << endl;
    for (map<pair<int,int>, int>::const_iterator it = allocations.begin();
         it != allocations.end(); ++it) {
        cout << "  (" << setw(2) << it->first.first << " -> " << setw(2) << it->first.second 
             << ") : Wavelength " << it->second << endl;
    }
    
    cout << "============================================\n" << endl;
}
