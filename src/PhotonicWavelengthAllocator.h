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

#ifndef __NOXIM_PHOTONIC_WAVELENGTH_ALLOCATOR_H__
#define __NOXIM_PHOTONIC_WAVELENGTH_ALLOCATOR_H__

#include <systemc.h>
#include <map>
#include <vector>
#include <set>
#include <queue>
#include <utility>
#include <cassert>
#include "DataStructs.h"
#include "GlobalParams.h"

using namespace std;

// ==================================================================================
// Data Structures for ORNoC-based Wavelength Allocation
// ==================================================================================

/**
 * @brief Represents a single communication flow in the ORNoC
 * 
 * A communication is identified by (src, dst) pair and is assigned a wavelength
 * and a ring connection to avoid contention
 */
struct ORNoC_Communication {
    int src;                                 // Source hub ID
    int dst;                                 // Destination hub ID
    bool connectivity;                       // True if direct photonic link exists
    
    int ring;                                // Assigned ring ID (-1 if unassigned)
    vector<pair<int,int>> path;              // Path as list of (src_hub, dst_hub) portions
    int wavelength;                          // Assigned wavelength ID (-1 if unassigned)
    
    bool processed;                          // Flag: communication has been processed
    
    // Constructor
    ORNoC_Communication() 
        : src(-1), dst(-1), connectivity(false), ring(-1), wavelength(-1), processed(false) {}
    
    ORNoC_Communication(int s, int d) 
        : src(s), dst(d), connectivity(false), ring(-1), wavelength(-1), processed(false) {}
};

/**
 * @brief Represents a photonic ring in the ORNoC topology
 * 
 * Each ring supports multiple wavelengths. Occupancy is tracked per (srcHub, dstHub) 
 * portion to detect wavelength contention
 */
struct PhotonicRing {
    int id;                                  // Ring ID
    bool clockwise;                          // True if ring direction is clockwise
    int max_wavelengths;                     // Number of wavelengths supported on this ring
    
    // occupancy[portion][(wavelength)] = true if wavelength is occupied
    // portion = (srcHub, dstHub)
    map<pair<int,int>, vector<bool>> occupation;
    
    // Constructor
    PhotonicRing()
        : id(-1), clockwise(true), max_wavelengths(GlobalParams::max_photonic_wavelengths) {}
    
    PhotonicRing(int ring_id, int max_wl, bool cw = true)
        : id(ring_id), clockwise(cw), max_wavelengths(max_wl) {}
};

// ==================================================================================
// PhotonicWavelengthAllocator Class
// ==================================================================================

class PhotonicWavelengthAllocator {
  public:
    /**
     * @brief Constructor
     * @param num_hubs Total number of photonic hubs in the system
     * @param max_wl Maximum number of wavelengths per ring
     */
    PhotonicWavelengthAllocator(int num_hubs, int max_wl);
    
    /**
     * @brief Destructor
     */
    ~PhotonicWavelengthAllocator();
    
    /**
     * @brief Main ORNoC generation function
     * 
     * Implements the core wavelength allocation algorithm:
     * - Builds communication matrix from photonic hub connectivity
     * - Iteratively creates rings and assigns wavelengths
     * - Alternates ring direction to balance load
     * 
     * @param max_wavelengths Maximum number of wavelengths to use
     */
    void generateORNoC(int max_wavelengths);
    
    /**
     * @brief Get allocated wavelength for a communication pair
     * @param src_hub Source hub ID
     * @param dst_hub Destination hub ID
     * @return Wavelength ID, or -1 if no allocation found
     */
    int getWavelength(int src_hub, int dst_hub) const;
    
    /**
     * @brief Get allocated ring for a communication pair
     * @param src_hub Source hub ID
     * @param dst_hub Destination hub ID
     * @return Ring ID, or -1 if no allocation found
     */
    int getRing(int src_hub, int dst_hub) const;
    
    /**
     * @brief Check if a wavelength is available on a ring portion
     * @param ring_id Ring ID
     * @param src_hub Source hub of portion
     * @param dst_hub Destination hub of portion
     * @param wavelength Wavelength ID to check
     * @return True if available, false otherwise
     */
    bool isWavelengthAvailable(int ring_id, int src_hub, int dst_hub, int wavelength) const;
    
    /**
     * @brief Reserve a wavelength on a ring portion
     * @param ring_id Ring ID
     * @param src_hub Source hub of portion
     * @param dst_hub Destination hub of portion
     * @param wavelength Wavelength ID to reserve
     * @return True if successfully reserved, false if unavailable
     */
    bool reserveWavelength(int ring_id, int src_hub, int dst_hub, int wavelength);
    
    /**
     * @brief Get total number of rings created
     * @return Number of rings
     */
    int getNumRings() const { return rings.size(); }
    
    /**
     * @brief Get total number of allocations
     * @return Number of communications allocated
     */
    int getNumAllocations() const { return allocations.size(); }
    
    /**
     * @brief Reset allocator state (for multiple runs)
     */
    void reset();
    
    /**
     * @brief Print allocation statistics (debug/logging)
     */
    void printStatistics() const;

  protected:
    /**
     * @brief Populate the connectivity matrix from GlobalParams
     * 
     * Builds the communication matrix by examining which hubs have
     * photonic channels connecting them
     */
    void populateConnectivity();
    
    /**
     * @brief Compute ring-based path from src to dst hub
     * 
     * Returns the portions of a path when traversing a ring topology
     * For simple 1-to-1 connections, path has single (src, dst) pair
     * For multi-hop, path has multiple portions
     * 
     * @param src Source hub
     * @param dst Destination hub
     * @return Vector of (srcHub, dstHub) portions composing the path
     */
    vector<pair<int,int>> getPortions(int src, int dst) const;
    
    /**
     * @brief Find first available wavelength for a communication
     * 
     * Checks all wavelengths on all portions of the path and returns
     * the first one that is free on all portions
     * 
     * @param ring_id Ring to search
     * @param portions Path portions to check
     * @return Wavelength ID if found, -1 otherwise
     */
    int findAvailableWavelength(int ring_id, const vector<pair<int,int>>& portions) const;
    
    /**
     * @brief Reserve a wavelength on all portions of a path
     * 
     * Atomically reserves a wavelength on all (src, dst) portions
     * 
     * @param ring_id Ring ID
     * @param portions Path portions to reserve
     * @param wavelength Wavelength ID to reserve
     * @return True if successful
     */
    bool reservePathWavelength(int ring_id, const vector<pair<int,int>>& portions, int wavelength);

  private:
    int num_hubs;                             // Total number of photonic hubs
    int max_wavelengths_per_ring;             // Max wavelengths per ring
    
    // Communication info
    map<pair<int,int>, ORNoC_Communication> communications;  // All potential communications
    map<pair<int,int>, int> allocations;      // (src, dst) -> wavelength mapping
    
    // Ring info
    vector<PhotonicRing> rings;               // List of allocated rings
    
    // Connectivity matrix
    map<pair<int,int>, bool> connectivity_matrix;  // (src, dst) -> connectivity
    
    // ==================================================================================
    // Physical Ring Topology for 4x4 Grid
    // ==================================================================================
    // Predefined physical photonic ring sequence (16-cluster 4x4 grid)
    // Connects all clusters in snake-like pattern avoiding crossings
    static const vector<int> physical_ring_sequence;
    
    // Maps hub ID to its position in the physical ring sequence
    // Used for fast path computation
    map<int, int> ring_position;
    
    /**
     * @brief Initialize the ring position map from physical ring sequence
     * Maps each hub_id to its position in the ring for O(1) lookups
     */
    void initializeRingPositionMap();
    
    /**
     * @brief Compute the exact path following the predefined physical ring topology
     * 
     * Follows the physical ring unidirectionally from src to dst,
     * generating all traversed portions along the ring.
     * 
     * @param src Source hub ID
     * @param dst Destination hub ID
     * @return Vector of (src_hub, dst_hub) portions following ring order
     */
    vector<pair<int,int>> computePhysicalRingPath(int src, int dst) const;
    
    // Utility functions
    pair<int,int> canonicalizeCommunication(int src, int dst) const {
        return (src < dst) ? make_pair(src, dst) : make_pair(dst, src);
    }
};

#endif // __NOXIM_PHOTONIC_WAVELENGTH_ALLOCATOR_H__
