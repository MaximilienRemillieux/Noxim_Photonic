/*
 * ORNoC WAVELENGTH ALLOCATION - QUICK REFERENCE GUIDE
 * 
 * This file provides quick copy-paste usage examples for integrating
 * the ORNoC wavelength allocation algorithm into Noxim simulations.
 */

================================================================================
BASIC USAGE IN SIMULATION FRAMEWORK
================================================================================
```cpp
Example 1: Minimal Setup (in main() or simulation initialization)
------

#include "PhotonicWavelengthAllocator.h"

// After reading configuration and creating hubs:

if (GlobalParams::use_wavelength_allocator) {
    int num_hubs = GlobalParams::photonic_hub_configuration.size();
    int max_wl = GlobalParams::max_photonic_wavelengths;
    
    PhotonicHub::wavelength_allocator = new PhotonicWavelengthAllocator(
        num_hubs, 
        max_wl
    );
    
    // Generate ORNoC allocation
    PhotonicHub::wavelength_allocator->generateORNoC(max_wl);
    
    // Print statistics
    PhotonicHub::wavelength_allocator->printStatistics();
}
```
========

Example 2: Query Allocations During Simulation
------
```cpp
// In any code that has access to allocator:

int src_hub = 0;
int dst_hub = 2;

int wl = PhotonicHub::wavelength_allocator->getWavelength(src_hub, dst_hub);
cout << "Wavelength for (" << src_hub << "->" << dst_hub << "): " << wl << endl;

int ring = PhotonicHub::wavelength_allocator->getRing(src_hub, dst_hub);
cout << "Ring for (" << src_hub << "->" << dst_hub << "): " << ring << endl;
```
========

Example 3: Check Wavelength Availability
------
```cpp
int ring_id = 0;
int src = 0;
int dst = 1;
int wavelength = 3;

bool available = PhotonicHub::wavelength_allocator->isWavelengthAvailable(
    ring_id, src, dst, wavelength
);

if (available) {
    cout << "Wavelength " << wavelength << " is available on ring " << ring_id << endl;
}
```
========

Example 4: Manual Wavelength Reservation
------
```cpp
int ring_id = 0;
int src_hub = 0;
int dst_hub = 1;
int wavelength = 2;

bool reserved = PhotonicHub::wavelength_allocator->reserveWavelength(
    ring_id, src_hub, dst_hub, wavelength
);

if (reserved) {
    cout << "Successfully reserved wavelength " << wavelength << endl;
} else {
    cout << "Failed to reserve wavelength (already in use or invalid)" << endl;
}
```
========

Example 5: Print Statistics and Debug Info
------
```cpp
// At end of simulation or during checkpoints:

cout << "Total rings allocated: " 
     << PhotonicHub::wavelength_allocator->getNumRings() << endl;

cout << "Total communications allocated: " 
     << PhotonicHub::wavelength_allocator->getNumAllocations() << endl;

// Detailed statistics:
PhotonicHub::wavelength_allocator->printStatistics();
```
========

================================================================================
YAML CONFIGURATION EXAMPLES
================================================================================

Example 1: Enable ORNoC Wavelength Allocation
------

# In config.yaml or similar:

simulation_time: 1000

# ... other parameters ...

# Photonic NoC parameters
use_photonic: true
use_wavelength_allocator: true
max_photonic_wavelengths: 8

# Hub configuration with photonic connectivity
photonic_hub_configuration:
  0:
    attachedNodesPhotonic: [0, 1, 2, 3]
    rxPhotonicChannels: [0, 1, 2, 3]
    txPhotonicChannels: [0, 1, 2, 3]
    wavelengths: [0, 1, 2, 3]
    toTileBufferSizePhotonic: 8
    fromTileBufferSizePhotonic: 8
    txBufferSizePhotonic: 4
    rxBufferSizePhotonic: 4
  
  1:
    attachedNodesPhotonic: [4, 5, 6, 7]
    rxPhotonicChannels: [4, 5, 6, 7]
    txPhotonicChannels: [4, 5, 6, 7]
    wavelengths: [4, 5, 6, 7]
    toTileBufferSizePhotonic: 8
    fromTileBufferSizePhotonic: 8
    txBufferSizePhotonic: 4
    rxBufferSizePhotonic: 4
  
  # ... more hubs as needed ...

========

================================================================================
FLIT STRUCTURE USAGE
================================================================================

New Fields in Flit struct:
```cpp
  int wavelength_id;      // Allocated wavelength (-1 if not assigned)
  int photonic_ring_id;   // Allocated ring (-1 if not assigned)

Initialization (automatic in ProcessingElement):
  flit.wavelength_id = -1;
  flit.photonic_ring_id = -1;

Updated during transmission in PhotonicHub::tileToPhotonicProcess():
  flit.wavelength_id = photonic_channel;
  flit.photonic_ring_id = wavelength_allocator->getRing(src, dst);

Usage example in custom routing logic:
  if (flit.wavelength_id >= 0) {
      // Flit has wavelength allocation
      int wl = flit.wavelength_id;
      int ring = flit.photonic_ring_id;
      // Use wavelength info for routing decisions
  }
```
========

================================================================================
ALGORITHM CONTROL & TUNING
================================================================================

Control via GlobalParams:
```cpp
  // Enable/disable ORNoC at runtime
  GlobalParams::use_wavelength_allocator = true;  // or false
  
  // Set wavelength capacity per ring
  GlobalParams::max_photonic_wavelengths = 8;     // typical: 4-16

Control via selectWavelength():

  // The enhanced selectWavelength() method automatically:
  // 1. Checks if allocator is enabled
  // 2. Queries allocator for allocation
  // 3. Falls back to random selection if:
  //    - Allocator disabled, or
  //    - Allocator returns -1 (no allocation)

  // No code changes needed - works transparently!

Performance Tuning:

  - Increase max_photonic_wavelengths for:
    * More ring creation overhead
    * Better spreading of communications
    * Higher photonic power consumption
  
  - Decrease max_photonic_wavelengths for:
    * Fewer rings (simpler topology)
    * More wavelength contention
    * Lower power consumption
```
========

================================================================================
DEBUGGING & MONITORING
================================================================================

Enable Detailed Logging:
```makefile
  1. Edit bin/Makefile:
     OLD: CXXFLAGS := $(OPT) $(OTHER) $(DEBUG)
     NEW: CXXFLAGS := $(OPT) $(OTHER) $(DEBUG) -DDEBUG -g
  
  2. Rebuild:
     cd bin && make
```
Monitor Flit State:
```cpp
  // In PhotonicHub.cpp or custom monitor:
  cout << "Flit " << flit.src_id << "->" << flit.dst_id
       << " WL=" << flit.wavelength_id
       << " Ring=" << flit.photonic_ring_id << endl;

Print Ring Information:

  // Access internal ring data via public methods:
  for (int r = 0; r < allocator->getNumRings(); r++) {
      // Check wavelength availability for all portions
      for (int src = 0; src < num_hubs; src++) {
          for (int dst = 0; dst < num_hubs; dst++) {
              for (int wl = 0; wl < max_wl; wl++) {
                  bool avail = allocator->isWavelengthAvailable(r, src, dst, wl);
                  if (!avail) {
                      cout << "Ring " << r << " ("<<src<<"->"<<dst
                           << ") WL " << wl << " RESERVED" << endl;
                  }
              }
          }
      }
  }

Track Allocations:

  // Get allocation statistics:
  cout << "Allocations: " << allocator->getNumAllocations() << endl;
  
  for (int src = 0; src < num_hubs; src++) {
      for (int dst = 0; dst < num_hubs; dst++) {
          int wl = allocator->getWavelength(src, dst);
          if (wl >= 0) {
              int ring = allocator->getRing(src, dst);
              cout << "(" << src << "->" << dst << ") -> WL " 
                   << wl << " on Ring " << ring << endl;
          }
      }
  }
```
========

================================================================================
COMMON SCENARIOS
================================================================================

Scenario 1: 2-Hub Photonic NoC with WDM
------

Configuration:
  - 2 photonic hubs
  - 8 wavelengths per link
  - Each hub connected to 4 tiles

Expected behavior:
  - generateORNoC creates 1 ring
  - Single communication (hub0 -> hub1) allocated WL 0
  - Reverse communication (hub1 -> hub0) might share or use different ring

Result:
  selectWavelength(0, 1) -> 0
  selectWavelength(1, 0) -> 0 (if same ring) or X (if different ring)

========

Scenario 2: Large Multi-Hub Photonic Topology
------

Configuration:
  - 8 photonic hubs (arranged in 2x4 grid)
  - 4 wavelengths per ring
  - Full mesh connectivity

Expected behavior:
  - generateORNoC creates multiple rings
  - Each ring handles subset of hub-pair communications
  - Wavelength allocation prevents contention on each ring
  - Alternative rings available if allocation fails

Typical output:
  Rings created: 3-4
  Wavelengths per ring: 4
  Total allocations: 28-32 (depends on hub pairs)

========

Scenario 3: ORNoC Disabled (Backward Compatibility)
------

Configuration:
  use_wavelength_allocator: false

Behavior:
  - selectWavelength() ignores allocator
  - Falls back to random channel selection
  - Flit fields remain initialized (-1)
  - No ORNoC overhead

Result:
  Original Noxim behavior preserved
  Great for validation/comparison

========

================================================================================
TROUBLESHOOTING
================================================================================

Problem: getWavelength() returns -1

Solution:
  1. Verify Global Params::use_wavelength_allocator = true
  2. Verify allocator != NULL
  3. Verify hub_pair has photonic connectivity
  4. Call printStatistics() to see allocations

========

Problem: Wavelength contention still occurs

Solution:
  1. Increase max_photonic_wavelengths
  2. Check connectivity matrix - may have unexpected connections
  3. Verify ring has sufficient capacity
  4. Check if rollback occurred during reservation

========

Problem: Memory corruption or segmentation fault

Solution:
  1. Verify num_hubs > 0 in constructor
  2. Verify max_wl > 0 (auto-corrected to 1)
  3. Check ring_id bounds before queries
  4. Verify hub IDs are in valid range
  5. Use safe_getWavelength() wrapper with bounds checking

Example safe wrapper:
```cpp
  int safe_getWavelength(int src, int dst) {
      if (src < 0 || dst < 0 || !allocator) return -1;
      return allocator->getWavelength(src, dst);
  }
```
========

Problem: Compilation error about missing includes

Solution:
  - Verify PhotonicWavelengthAllocator.h is in src/ directory
  - Verify #include "PhotonicWavelengthAllocator.h" in PhotonicHub.h
  - Recompile: cd bin && make clean && make

========

================================================================================
PERFORMANCE PROFILING
================================================================================

Measure ORNoC Generation Time:
```cpp
  clock_t start = clock();
  allocator->generateORNoC(max_wl);
  clock_t end = clock();
  
  double elapsed = double(end - start) / CLOCKS_PER_SEC;
  cout << "ORNoC generation: " << elapsed << " seconds" << endl;

Measure Query Performance:

  clock_t start = clock();
  for (int i = 0; i < 100000; i++) {
      int wl = allocator->getWavelength(0, 1);
  }
  clock_t end = clock();
  
  double elapsed = double(end - start) / CLOCKS_PER_SEC;
  cout << "100k queries: " << elapsed << " seconds" << endl;
  cout << "Per-query: " << (elapsed * 1000000 / 100000) << " microseconds" << endl;

Expected results:
  - ORNoC generation: O(1-10ms) for typical topologies
  - getWavelength() queries: O(0.1-1 nanosecond) - negligible
```
========

================================================================================
