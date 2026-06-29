/*
 * ORNoC WAVELENGTH ALLOCATION ALGORITHM - IMPLEMENTATION SUMMARY
 * 
 * Integration Guide for Noxim Photonic NoC with ORNoC Wavelength Allocation
 * 
 * This document describes the wavelength allocation implementation based on the
 * ORNoC (Optical Ring Network-on-Chip) methodology for the Noxim simulator.
 */
```cpp
================================================================================
1. NEW FILES CREATED
================================================================================

PhotonicWavelengthAllocator.h
PhotonicWavelengthAllocator.cpp
  
  Location: /home/maximilien/noxim/src/
  
  These files implement the core ORNoC wavelength allocation algorithm with:
  - ORNoC_Communication struct: Represents individual communication flows
  - PhotonicRing struct: Represents virtual photonic rings with wavelength tracking
  - PhotonicWavelengthAllocator class: Main allocator implementing the algorithm
  
  Key Methods:
    * generateORNoC(max_wavelengths)
      - Main algorithm entry point
      - Builds communication matrix
      - Iteratively creates rings and assigns wavelengths
      - Alternates ring direction for load balancing
    
    * getWavelength(src_hub, dst_hub)
      - Query method for retrieving allocated wavelength
      - Returns -1 if no allocation found
    
    * getRing(src_hub, dst_hub)  
      - Query method for retrieving allocated ring
      - Returns -1 if not allocated
    
    * reserveWavelength(ring_id, src_hub, dst_hub, wavelength)
      - Atomically reserves wavelength on a ring portion
      - Prevents contention
    
  Features:
    - Safe map access (no std::map::at exceptions)
    - Atomic multi-portion reservations
    - Ring direction alternation
    - Comprehensive statistics reporting

================================================================================
2. MODIFIED FILES - DETAILED CHANGES
================================================================================

--- File: DataStructs.h ---

CHANGE 1.1: Extended Flit structure with photonic fields
  Location: Around line 120-128
  
  OLD:
    int hub_relay_node;
    
    inline bool operator ==(const Flit & flit) const {
        return (flit.src_id == src_id && flit.dst_id == dst_id
                && ... && flit.use_low_voltage_path == use_low_voltage_path);
    
  NEW:
    int hub_relay_node;
    
    // Photonic wavelength allocation (ORNoC)
    int wavelength_id;      // Allocated wavelength for photonic transmission (-1 if not used)
    int photonic_ring_id;   // Allocated ring for photonic transmission (-1 if not used)
    
    inline bool operator ==(const Flit & flit) const {
        return (flit.src_id == src_id && flit.dst_id == dst_id
                && ... && flit.use_low_voltage_path == use_low_voltage_path
                && flit.wavelength_id == wavelength_id
                && flit.photonic_ring_id == photonic_ring_id);
  
  Rationale: Adds per-flit wavelength allocation tracking for:
    - Ring-based photonic transmission
    - Multi-hop photonic communication
    - Wavelength contention detection

--- File: GlobalParams.h ---

CHANGE 2.1: Added ORNoC wavelength allocation parameters
  Location: Before PowerConfig declaration (around line 227)
  
  NEW:
    // Wavelength (ORNoC) allocation parameters
    static bool use_wavelength_allocator;    // Enable ORNoC wavelength allocation
    static int max_photonic_wavelengths;     // Max wavelengths per photonic ring
  
  Rationale: 
    - use_wavelength_allocator: Runtime control flag for enabling ORNoC
    - max_photonic_wavelengths: Configuration parameter for wavelength budget

--- File: GlobalParams.cpp ---

CHANGE 3.1: Initialize ORNoC parameters
  Location: After photonic_hub_for_tile declaration (around line 71)
  
  NEW:
    // Wavelength (ORNoC) allocation parameters
    bool GlobalParams::use_wavelength_allocator = false;
    int GlobalParams::max_photonic_wavelengths = 8;
  
  Rationale:
    - Default disabled to maintain backward compatibility
    - Default 8 wavelengths per ring for typical WDM photonic NoCs

--- File: PhotonicHub.h ---

CHANGE 4.1: Added PhotonicWavelengthAllocator include
  Location: Line 24 (before "using namespace std")
  
  NEW:
    #include "PhotonicWavelengthAllocator.h"
  
  Rationale: Enables PhotonicHub to access allocator interface

CHANGE 4.2: Added static allocator pointer
  Location: After "Power power;" declaration (around line 87)
  
  NEW:
    // Wavelength allocator (ORNoC) - shared across all PhotonicHub instances
    static PhotonicWavelengthAllocator* wavelength_allocator;
  
  Rationale:
    - Static because allocator is global to all hubs
    - Pointer for dynamic allocation/deallocation control
    - Allows all hub instances to access same allocation table

--- File: PhotonicHub.cpp ---

CHANGE 5.1: Initialize static allocator pointer
  Location: After "#include "PhotonicHub.h"" (line 13)
  
  NEW:
    // Initialize static wavelength allocator (ORNoC)
    PhotonicWavelengthAllocator* PhotonicHub::wavelength_allocator = NULL;
  
  Rationale: Initialize at NULL, set by user/simulation framework

CHANGE 5.2: Enhanced selectWavelength() method
  Location: Line 565+
  
  OLD:
    int PhotonicHub::selectWavelength(int src_hub, int dst_hub) const
    {
        // Random channel selection from intersection
        ...
    }
  
  NEW:
    int PhotonicHub::selectWavelength(int src_hub, int dst_hub) const
    {
        // If ORNoC wavelength allocator is enabled and available, use it
        if (GlobalParams::use_wavelength_allocator && wavelength_allocator != NULL) {
            int wl = wavelength_allocator->getWavelength(src_hub, dst_hub);
            if (wl >= 0) {
                return wl;  // Use allocated wavelength
            }
        }
        
        // Fallback to original random channel selection
        ...
    }
  
  Rationale: 
    - First tries allocator if enabled
    - Falls back to original random behavior for compatibility
    - No performance impact when allocator disabled

CHANGE 5.3: Set wavelength fields on flit before transmission
  Location: In tileToPhotonicProcess() forwarding phase (around line 464)
  
  OLD:
    if (!(init[photonic_channel]->buffer_tx.IsFull()) )
    {
        buffer_from_tile[i][vc].Pop();
        power.bufferFromTilePop();
        init[photonic_channel]->buffer_tx.Push(flit);
        ...
    }
  
  NEW:
    if (!(init[photonic_channel]->buffer_tx.IsFull()) )
    {
        // Set wavelength allocation (ORNoC) fields on the flit
        flit.wavelength_id = photonic_channel;
        if (GlobalParams::use_wavelength_allocator && wavelength_allocator != NULL) {
            int dst_hub = (flit.hub_relay_node != NOT_VALID) ? 
                tile2PhotonicHub(flit.hub_relay_node) : 
                tile2PhotonicHub(flit.dst_id);
            flit.photonic_ring_id = wavelength_allocator->getRing(local_id, dst_hub);
        }
        
        buffer_from_tile[i][vc].Pop();
        power.bufferFromTilePop();
        init[photonic_channel]->buffer_tx.Push(flit);
        ...
    }
  
  Rationale:
    - Marks each flit with its allocated wavelength and ring
    - Enables wavelength-aware routing at receiving end
    - Supports multi-ring topologies

--- File: ProcessingElement.cpp ---

CHANGE 6.1: Initialize wavelength fields in Flit generation
  Location: In generateFlit() method (around line 116)
  
  OLD:
    flit.hub_relay_node = NOT_VALID;
    
    if (packet.size == packet.flit_left)
  
  NEW:
    flit.hub_relay_node = NOT_VALID;
    
    // Initialize photonic wavelength allocation fields (ORNoC)
    flit.wavelength_id = -1;
    flit.photonic_ring_id = -1;
    
    if (packet.size == packet.flit_left)
  
  Rationale:
    - Ensures all flits start with invalid wavelength allocation
    - Prevents undefined behavior
    - Per-tile flit generation point

================================================================================
3. ALGORITHM FLOW
================================================================================

Step 1: Initialization Phase
  - Framework creates PhotonicWavelengthAllocator instance
  - Calls generateORNoC(max_wavelengths)
  - Populates connectivity matrix from GlobalParams::photonic_hub_configuration

Step 2: Communication Matrix Building
  - Scans all photonic hub configurations
  - Identifies all potential hub-to-hub communications
  - Creates ORNoC_Communication entries for each connection

Step 3: Ring Creation Loop
  - Iterates over unprocessed communications
  - For each communication:
    1. Compute ring path (portions)
    2. Try to allocate to existing rings
    3. If no space on existing rings, create new ring
    4. Allocate first available wavelength
    5. Reserve wavelength on all path portions

Step 4: Runtime Wavelength Selection
  - When PhotonicHub needs to send flit:
    * Calls selectWavelength(src_hub, dst_hub)
    * If allocator enabled, queries allocator.getWavelength()
    * Otherwise falls back to random selection
    * Return value is photonic_channel/wavelength_id

Step 5: Flit Transmission
  - Flit marked with:
    * wavelength_id: Selected wavelength
    * photonic_ring_id: Assigned ring
  - Traverses photonic network via InitiatorPhotonic/TargetPhotonic
  - Receiver can verify wavelength correctness if needed

================================================================================
4. KEY INTEGRATION POINTS
================================================================================

Point 1: Allocator Lifecycle
  - Created by: Simulation framework or NoC instantiation code
  - Lifetime: For duration of simulation
  - Access: Via static PhotonicHub::wavelength_allocator pointer
  
  Example usage:
    int num_hubs = GlobalParams::photonic_hub_configuration.size();
    PhotonicHub::wavelength_allocator = new PhotonicWavelengthAllocator(
        num_hubs, 
        GlobalParams::max_photonic_wavelengths
    );
    PhotonicHub::wavelength_allocator->generateORNoC(
        GlobalParams::max_photonic_wavelengths
    );

Point 2: Flit Wavelength Tagging  
  - ProcessingElement: Creates flit with wavelength_id = -1
  - PhotonicHub: Updates wavelength_id before transmission
  - Allocator: Provides wavelength and ring information
  - InitiatorPhotonic: Sends tagged flit to target

Point 3: Safety Mechanisms
  - Safe map access: No std::map::at() which throws on missing key
  - Boundary checks: All indices validated before access
  - Fallback behavior: Works without allocator (backward compatible)
  - Atomic reservations: Multi-portion reserve-or-rollback operations

================================================================================
5. CONFIGURATION PARAMETERS
================================================================================

In YAML configuration or GlobalParams:

  use_wavelength_allocator: false        # Enable ORNoC allocation
  max_photonic_wavelengths: 8            # Wavelengths per ring

  photonic_hub_configuration:            # Hub connectivity
    <hub_id>:
      attachedNodesPhotonic: [...]       # Tiles attached to this hub
      rxPhotonicChannels: [...]          # RX wavelength/channel IDs
      txPhotonicChannels: [...]          # TX wavelength/channel IDs
      wavelengths: [...]                 # Available wavelengths
      ...

================================================================================
6. ERROR HANDLING & SAFEGUARDS
================================================================================

Input Validation:
  - num_hubs > 0: Enforced in constructor
  - max_wavelengths > 0: Forced to 1 if invalid
  - ring_id bounds checking before access
  - wavelength bounds checking before access

Memory Safety:
  - Using std::map with find() instead of at()
  - Iterator validation before dereferencing
  - Vector bounds checks before indexing
  - No fixed-size arrays (all dynamic containers)

Atomic Operations:
  - All multi-step operations are atomic
  - Rollback on partial failure
  - No inconsistent state possible

Backward Compatibility:
  - Works without allocator (wavelength_allocator == NULL)
  - Falls back to random channel selection
  - All new fields initialized to -1 (invalid)
  - Existing code unaffected if feature disabled

================================================================================
7. TESTING & DEBUGGING
================================================================================

Enable Logging:
  1. Add "-g -DDEBUG" to CXXFLAGS in bin/Makefile
  2. Recompile: make clean; make
  3. Run simulation - LOG macros in PhotonicWavelengthAllocator.cpp will print

Print Statistics:
  // In simulation framework:
  PhotonicHub::wavelength_allocator->printStatistics();
  
  Output shows:
    - Number of rings created
    - Number of allocations made
    - Number of wavelengths reserved per ring
    - Communication-to-wavelength mapping

Query Specific Allocations:
  int wl = PhotonicHub::wavelength_allocator->getWavelength(hub1, hub2);
  int ring = PhotonicHub::wavelength_allocator->getRing(hub1, hub2);

Verify Flit State:
  // In PhotonicHub/PhotonicChannel code:
  cout << "Flit wavelength: " << flit.wavelength_id
       << ", Ring: " << flit.photonic_ring_id << endl;

================================================================================
8. PERFORMANCE CHARACTERISTICS
================================================================================

Time Complexity (generateORNoC):
  - O(C * R * W * P) where:
    C = number of communications
    R = number of rings
    W = number of wavelengths per ring
    P = average path portions per communication
  - Typical: O(N^2) for N hubs in simple topology

Space Complexity:
  - O(N^2 * W) for communications and occupation maps
  - O(R * P) for ring occupation tracking
  - Typical: O(N^2) for N hubs

Runtime Overhead:
  - selectWavelength(): O(1) map lookup
  - getWavelength(): O(1) amortized
  - Near-zero impact when feature disabled

================================================================================
9. FUTURE EXTENSIONS
================================================================================

Possible enhancements:
  1. Multi-hop path computation for torus/mesh photonic topologies
  2. Dynamic wavelength allocation based on traffic patterns
  3. Wavelength defragmentation/optimization
  4. Support for multiple photonic rings per hub pair
  5. Integration with power.yaml for photonic component modeling
  6. Wavelength-aware routing algorithms for mesh NoCs
  7. Per-application wavelength reservation policies

================================================================================
10. KNOWN LIMITATIONS & NOTES
================================================================================

Current Implementation:
  - Supports single-hop ORNoC (direct hub connections)
  - Static allocation (not traffic-adaptive)
  - Simple ring creation strategy
  - No multi-hop wavelength path computation

Assumptions:
  - All hubs have equal number of wavelengths
  - Wavelength capacity >= number of communications
  - Photonic hubs fully connected via photonic channels
  - No wavelength conversion between rings

Compatibility:
  - Requires C++11 or later (std::map, vectors)
  - SystemC 2.3.x compatible
  - Works with existing Noxim router/hub infrastructure
  - No changes required to wireless NoC behavior

================================================================================
