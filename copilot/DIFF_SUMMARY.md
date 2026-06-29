/*
 * ORNoC WAVELENGTH ALLOCATION - DIFF SUMMARY
 * 
 * This document shows the exact changes made to each file in unified diff format.
 */

================================================================================
FILE: src/DataStructs.h
================================================================================
```cpp
Location: Line ~120-135 (Flit structure)

--- DataStructs.h (original)
+++ DataStructs.h (modified)
@@ Line 128,7 +128,12 @@
    int hop_no;			// Current number of hops from source to destination
    bool use_low_voltage_path;
 
    int hub_relay_node;
+    
+    // Photonic wavelength allocation (ORNoC)
+    int wavelength_id;      // Allocated wavelength for photonic transmission (-1 if not used)
+    int photonic_ring_id;   // Allocated ring for photonic transmission (-1 if not used)
 
    inline bool operator ==(const Flit & flit) const {
        return (flit.src_id == src_id && flit.dst_id == dst_id
@@ -136,7 +141,9 @@
            && flit.sequence_no == sequence_no
            && flit.sequence_length == sequence_length
            && flit.payload == payload && flit.timestamp == timestamp
            && flit.hop_no == hop_no
-           && flit.use_low_voltage_path == use_low_voltage_path);
+           && flit.use_low_voltage_path == use_low_voltage_path
+           && flit.wavelength_id == wavelength_id
+           && flit.photonic_ring_id == photonic_ring_id);
    }};
```
================================================================================
FILE: src/GlobalParams.h
================================================================================
```cpp
Location: Line ~225 (before PowerConfig and ascii_monitor)

--- GlobalParams.h (original)
+++ GlobalParams.h (modified)
@@ Line 220,6 +220,9 @@
    static map<int, PhotonicHubConfig> photonic_hub_configuration;
    static map<int, int> photonic_hub_for_tile;
 
+    // Wavelength (ORNoC) allocation parameters
+    static bool use_wavelength_allocator;    // Enable ORNoC wavelength allocation
+    static int max_photonic_wavelengths;     // Max wavelengths per photonic ring
 
    static PowerConfig power_configuration;
```
================================================================================
FILE: src/GlobalParams.cpp
================================================================================
```cpp
Location: Line ~71 (after photonic_hub_for_tile)

--- GlobalParams.cpp (original)
+++ GlobalParams.cpp (modified)
@@ Line 68,6 +68,10 @@
    map<int, PhotonicHubConfig> GlobalParams::photonic_hub_configuration;
    map<int,int> GlobalParams::photonic_hub_for_tile;
 
+    // Wavelength (ORNoC) allocation parameters
+    bool GlobalParams::use_wavelength_allocator = false;
+    int GlobalParams::max_photonic_wavelengths = 8;
+
    PowerConfig GlobalParams::power_configuration;
    // out of yaml configuration
    bool GlobalParams::ascii_monitor;
```
================================================================================
FILE: src/PhotonicHub.h
================================================================================
```cpp
Change 1: Line ~24 (after #include "Power.h")

--- PhotonicHub.h (original)
+++ PhotonicHub.h (modified)
@@ Line 22,6 +22,7 @@
    #include "TokenRing.h"
    #include "Power.h"
+   #include "PhotonicWavelengthAllocator.h"
 
    using namespace std;

Change 2: Line ~87 (after "Power power;")

--- PhotonicHub.h (original)
+++ PhotonicHub.h (modified)
@@ Line 84,6 +84,9 @@
 
    Power power;
+    
+    // Wavelength allocator (ORNoC) - shared across all PhotonicHub instances
+    static PhotonicWavelengthAllocator* wavelength_allocator;
 
    int total_sleep_cycles;
```
================================================================================
FILE: src/PhotonicHub.cpp
================================================================================
```cpp
Change 1: Line ~13 (after #include "PhotonicHub.h")

--- PhotonicHub.cpp (original)
+++ PhotonicHub.cpp (modified)
@@ Line 10,6 +10,9 @@
    */
    #include "PhotonicHub.h"
 
+   // Initialize static wavelength allocator (ORNoC)
+   PhotonicWavelengthAllocator* PhotonicHub::wavelength_allocator = NULL;
+
    int PhotonicHub::tile2Port(int id)

Change 2: Line ~565-572 (selectWavelength method)

--- PhotonicHub.cpp (original)
+++ PhotonicHub.cpp (modified)
@@ Line 565,6 +565,14 @@
    int PhotonicHub::selectWavelength(int src_hub, int dst_hub) const
    {
+       // If ORNoC wavelength allocator is enabled and available, use it
+       if (GlobalParams::use_wavelength_allocator && wavelength_allocator != NULL) {
+           int wl = wavelength_allocator->getWavelength(src_hub, dst_hub);
+           if (wl >= 0) {
+               return wl;  // Use allocated wavelength
+           }
+       }
+       
+       // Fallback to original random channel selection
        vector<int> & first = GlobalParams::photonic_hub_configuration[src_hub].txPhotonicChannels;
        vector<int> & second = GlobalParams::photonic_hub_configuration[dst_hub].rxPhotonicChannels;

Change 3: Line ~462-481 (tileToPhotonicProcess, 2nd phase forwarding)

--- PhotonicHub.cpp (original)
+++ PhotonicHub.cpp (modified)
@@ Line 459,13 +459,21 @@
             if (!buffer_from_tile[i][vc].IsEmpty())
             {
                 Flit flit = buffer_from_tile[i][vc].Front();
-                // powerFront already accounted in 1st phase
+                // powerFront already accounted in 1st phase
 
                 assert(r_from_tile[i][vc] == DIRECTION_WAVEGUIDE);
 
                 int photonic_channel =  o;
 
                 if (photonic_channel != NOT_RESERVED)
                 {
                     if (!(init[photonic_channel]->buffer_tx.IsFull()) )
                     {
+                        // Set wavelength allocation (ORNoC) fields on the flit
+                        flit.wavelength_id = photonic_channel;
+                        if (GlobalParams::use_wavelength_allocator && wavelength_allocator != NULL) {
+                            int dst_hub = (flit.hub_relay_node != NOT_VALID) ? 
+                                tile2PhotonicHub(flit.hub_relay_node) : 
+                                tile2PhotonicHub(flit.dst_id);
+                            flit.photonic_ring_id = wavelength_allocator->getRing(local_id, dst_hub);
+                        }
+                        
                         buffer_from_tile[i][vc].Pop();
                         power.bufferFromTilePop();
                         init[photonic_channel]->buffer_tx.Push(flit);
```
================================================================================
FILE: src/ProcessingElement.cpp
================================================================================
```cpp
Location: Line ~113-120 (in generateFlit() method)

--- ProcessingElement.cpp (original)
+++ ProcessingElement.cpp (modified)
@@ Line 113,6 +113,10 @@
    flit.timestamp = packet.timestamp;
    flit.sequence_no = packet.size - packet.flit_left;
    flit.sequence_length = packet.size;
    flit.hop_no = 0;
    //  flit.payload     = DEFAULT_PAYLOAD;
 
    flit.hub_relay_node = NOT_VALID;
+   
+   // Initialize photonic wavelength allocation fields (ORNoC)
+   flit.wavelength_id = -1;
+   flit.photonic_ring_id = -1;
 
    if (packet.size == packet.flit_left)
```
================================================================================
NEWLY CREATED FILES
================================================================================

File: src/PhotonicWavelengthAllocator.h
  - ~220 lines
  - Contains: Data structures (ORNoC_Communication, PhotonicRing)
  - Contains: PhotonicWavelengthAllocator class definition and methods

File: src/PhotonicWavelengthAllocator.cpp
  - ~450+ lines
  - Contains: Complete algorithm implementation
  - Contains: Safe container access, atomic operations, statistics

File: ORNOC_IMPLEMENTATION_GUIDE.txt (this directory)
  - Complete integration documentation
  - Algorithm flow explanation
  - Safety mechanisms and error handling
  - Testing and debugging guidance

File: ORNOC_QUICK_REFERENCE.txt (this directory)
  - Usage examples with copy-paste code
  - Configuration examples
  - Common scenarios and troubleshooting
  - Performance profiling tips

File: DIFF_SUMMARY.txt (this file)
  - Unified diff format for each file
  - Line-by-line change documentation

================================================================================
SUMMARY OF CHANGES
================================================================================

Total files modified: 6
  - DataStructs.h: +5 lines (Flit fields + operator==)
  - GlobalParams.h: +3 lines (new static members)
  - GlobalParams.cpp: +3 lines (initialization)
  - PhotonicHub.h: +3 lines (include + allocator pointer)
  - PhotonicHub.cpp: +40 lines (static init + selectWavelength + flit tagging)
  - ProcessingElement.cpp: +4 lines (field initialization)

Total files created: 4
  - PhotonicWavelengthAllocator.h: ~220 lines
  - PhotonicWavelengthAllocator.cpp: ~450+ lines
  - ORNOC_IMPLEMENTATION_GUIDE.txt: ~420 lines
  - ORNOC_QUICK_REFERENCE.txt: ~320 lines

Total new code: ~1,400 lines of algorithmic logic and documentation

Integration points: 8
  1. DataStructs.h Flit extension
  2. GlobalParams params
  3. GlobalParams.cpp initialization
  4. PhotonicHub.h allocator reference
  5. PhotonicHub.cpp allocator initialization
  6. PhotonicHub.cpp selectWavelength enhancement
  7. PhotonicHub.cpp flit wavelength tagging
  8. ProcessingElement.cpp flit field init

================================================================================
BACKWARD COMPATIBILITY ANALYSIS
================================================================================

✓ Fully Backward Compatible:
  - use_wavelength_allocator defaults to false
  - selectWavelength() falls back to random selection
  - Flit fields initialized to -1 (invalid)
  - No changes to existing Noxim data flow
  - Wireless NoC behavior untouched

Migration Path:
  1. Apply all changes (no action needed, default off)
  2. Set use_wavelength_allocator = true to enable
  3. Create/initialize allocator in simulation main
  4. No other code changes required

Verification:
  - Disable feature (use_wavelength_allocator = false)
  - Run existing test suite
  - Results should match original Noxim

================================================================================
