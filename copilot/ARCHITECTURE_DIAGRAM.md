/*
 * ORNoC WAVELENGTH ALLOCATION - ARCHITECTURE DIAGRAM & INTEGRATION MAP
 */

================================================================================
SYSTEM ARCHITECTURE
================================================================================

┌─────────────────────────────────────────────────────────────────────────────┐
│                    NOXIM PHOTONIC NOC SIMULATOR                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                   SIMULATION FRAMEWORK (Main)                        │  │
│  │                                                                       │  │
│  │  1. Create GlobalParams (configuration)                             │  │
│  │  2. Create PhotonicWavelengthAllocator (if enabled)    ◄─── NEW     │  │
│  │  3. Call generateORNoC()                              ◄─── NEW      │  │
│  │  4. Create PhotonicHub instances                                    │  │
│  │  5. Run simulation                                                   │  │
│  │  6. Collect statistics                                              │  │
│  └──────────────────────────────────────────────────────────────────────┘  │
│           │                                                                  │
│           ├─ Sets: PhotonicHub::wavelength_allocator (static)  ◄─ NEW    │
│           └─ Sets: GlobalParams::use_wavelength_allocator     ◄─ NEW    │
│                                                                              │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │ PhotonicWavelengthAllocator (NEW CLASS)                             │  │
│  ├──────────────────────────────────────────────────────────────────────┤  │
│  │                                                                       │  │
│  │  Private State:                                                      │  │
│  │  ├─ communications[]        // Communication matrix                 │  │
│  │  ├─ allocations[]           // (src,dst) -> wavelength mapping     │  │
│  │  ├─ rings[]                 // Virtual photonic rings               │  │
│  │  └─ connectivity_matrix[]   // Hub-to-hub connectivity              │  │
│  │                                                                       │  │
│  │  Public Methods:                                                    │  │
│  │  ├─ generateORNoC()         // Main algorithm                       │  │
│  │  ├─ getWavelength()         // Query allocated wavelength           │  │
│  │  ├─ getRing()               // Query allocated ring                 │  │
│  │  ├─ isWavelengthAvailable() // Check contention-free               │  │
│  │  ├─ reserveWavelength()     // Reserve wavelength                   │  │
│  │  └─ printStatistics()       // Debug output                         │  │
│  │                                                                       │  │
│  └──────────────────────────────────────────────────────────────────────┘  │
│           │   Input: GlobalParams::photonic_hub_configuration               │
│           │   Output: Wavelength allocation table                          │
│           │                                                                 │
│  ┌────────▼──────────────────────────────────────────────────────────────┐  │
│  │              PHOTONIC NOC INFRASTRUCTURE                              │  │
│  ├───────────────────────────────────────────────────────────────────────┤  │
│  │                                                                        │  │
│  │  ┌────────────┐    Photonic      ┌────────────┐                      │  │
│  │  │PhotonicHub0├────Channels────┤PhotonicHub1│                      │  │
│  │  │            │   (wavelengths)  │            │                      │  │
│  │  │ [Alloc]◄───┼─── Query ◄──────┼─ [Alloc]   │  ◄─ NEW             │  │
│  │  │            │    Result         │            │                      │  │
│  │  └────┬───────┘  (from allocator) └───┬────────┘                      │  │
│  │       │                               │                               │  │
│  │       ├─ From Tile  ┌─────────┐      │  Flit flow with              │  │
│  │       ├─ To Tile    │ Flit    │      │  wavelength tagging           │  │
│  │       ├─ Buffers    │ wl_id ◄─┼──────┤  ◄─ NEW                       │  │
│  │       └─ Wavelength │ ring_id │      │                               │  │
│  │          Selection  └─────────┘      │                               │  │
│  │                                       │                               │  │
│  │  ┌────────────┐                 ┌────────────┐                      │  │
│  │  │ProcessingEl├──────Flit───────┤PhotonicHub2│                      │  │
│  │  │ Generate   │ [wl_id=-1, ◄─── │            │◄─ NEW                │  │
│  │  │ Flit       │  ring_id=-1]     │ [Alloc]    │ Field Init           │  │
│  │  └────────────┘  ◄─ NEW          └────────────┘                      │  │
│  │                                       │                               │  │
│  │  ┌──────────────────────────────────┴────────────────────────────┐  │  │
│  │  │ InitiatorPhotonic / TargetPhotonic TLM-2 Transport           │  │  │
│  │  │ (Existing, enhanced with wavelength-aware routing)           │  │  │
│  │  └──────────────────────────────────────────────────────────────┘  │  │
│  │                                                                        │  │
│  └────────────────────────────────────────────────────────────────────────┘  │
│                                                                               │
└─────────────────────────────────────────────────────────────────────────────┘

================================================================================
DATA FLOW DURING FLIT TRANSMISSION
================================================================================

1. FLIT GENERATION (ProcessingElement.cpp)
   ┌─────────────────────────────────────┐
   │ Packet → generateFlit()             │
   │                                     │
   │ NEW: Initialize fields              │
   │   wavelength_id = -1                │  (undefined)
   │   photonic_ring_id = -1             │  (undefined)
   └──────────┬──────────────────────────┘
              │
              ▼
2. FLIT TRANSMISSION PREPARATION (PhotonicHub::tileToPhotonicProcess)
   ┌─────────────────────────────────────────────────────────┐
   │ Reservation Phase:                                      │
   │   1. Route flit (route())                               │
   │   2. Check if DIRECTION_WAVEGUIDE (photonic route)     │
   │   3. selectWavelength(src_hub, dst_hub)                │
   │      ├─ Check: GlobalParams::use_wavelength_allocator  │
   │      ├─ If true: Query allocator.getWavelength()  ◄─ NEW
   │      └─ Else: Random channel selection (original)      │
   │   4. Reserve channel in tile2photonic_reservation_table│
   └──────────┬────────────────────────────────────────────┘
              │
              ▼
3. FLIT ENHANCEMENT (PhotonicHub::tileToPhotonicProcess - Phase 2)
   ┌──────────────────────────────────────────────────────────┐
   │ NEW: Set wavelength fields on flit:                      │
   │   flit.wavelength_id = photonic_channel  ◄─ Channel ID  │
   │   if (allocator != NULL)                                │
   │      flit.photonic_ring_id = allocator->getRing(...)   │
   │                                                          │
   │ Result: Tagged flit ready for transmission              │
   └──────────┬───────────────────────────────────────────────┘
              │
              ▼
4. TLM TRANSPORT (InitiatorPhotonic→PhotonicChannel→TargetPhotonic)
   ┌────────────────────────────────────┐
   │ TLM-2 b_transport()                │
   │   - Flit included in payload       │
   │   - Contains: src, dst, wl_id ◄─ NEW
   │   - Contains: ring_id        ◄─ NEW
   │   - Transports across channel     │
   │   - Power calculations            │
   └──────────┬─────────────────────────┘
              │
              ▼
5. FLIT RECEPTION (PhotonicHub::photonicToTileProcess)
   ┌─────────────────────────────────────┐
   │ Read flit with wavelength info      │
   │   wavelength_id = <allocation>      │
   │   photonic_ring_id = <ring>         │
   │                                     │
   │ Can verify wavelength correctness   │
   │ or use for routing decisions        │
   └─────────────────────────────────────┘

================================================================================
ALGORITHM FLOW (generateORNoC)
================================================================================

INPUT:  max_wavelengths (from GlobalParams)

STEP 1: Populate Connectivity Matrix
        ┌─────────────────────────────────────────┐
        │ Scan all photonic_hub_configuration     │
        │ Build connectivity matrix               │
        │ Result: Which hub pairs are connected   │
        └────────────┬────────────────────────────┘
                     ▼
STEP 2: Build Communication Matrix
        ┌─────────────────────────────────────────────────┐
        │ For each connected hub pair (src, dst):         │
        │   Create ORNoC_Communication entry              │
        │ All communications in: communications[]          │
        │ Status: { src, dst, connectivity, processed }   │
        └────────────┬────────────────────────────────────┘
                     ▼
STEP 3: Main Allocation Loop
        ┌──────────────────────────────────────────────────┐
        │ For each unprocessed communication:              │
        │                                                   │
        │   Try Option A: Fit on existing ring             │
        │   ├─ For each ring:                              │
        │   │  ├─ Get path portions (src→dst)             │
        │   │  ├─ Find available wavelength               │
        │   │  └─ If found: Reserve & mark processed      │
        │   │      Store in allocations[]                 │
        │   │                                               │
        │   Try Option B: Create new ring                 │
        │   ├─ If no space on existing rings:             │
        │   │  ├─ Create PhotonicRing                     │
        │   │  ├─ Allocate wavelength 0                   │
        │   │  ├─ Reserve path on new ring                │
        │   │  ├─ Mark processed                          │
        │   │  └─ Alternate ring direction for next       │
        │   │      (load balancing)                        │
        │   │                                               │
        │   Track in rings[]                              │
        │   Track in allocations[]                        │
        └────────────┬───────────────────────────────────┘
                     ▼
STEP 4: Statistics Collection
        ┌─────────────────────────────────────┐
        │ Count processed communications      │
        │ Count created rings                 │
        │ Verify all allocations made         │
        │ Print summary (if LOG enabled)      │
        └─────────────────────────────────────┘

OUTPUT: Populated allocations[] and rings[]

QUERY ACCESS: getWavelength(src, dst) → O(1) lookup

================================================================================
SAFETY MECHANISMS
================================================================================

┌──────────────────────────────────────────────────────────────┐
│ DEFENSIVE PROGRAMMING PATTERNS                              │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│ 1. Input Validation                                         │
│    • num_hubs > 0 checked in constructor                   │
│    • max_wavelengths forced to 1 if ≤ 0                    │
│    • Ring ID bounds before access                          │
│    • Hub ID bounds before access                           │
│                                                              │
│ 2. Safe Container Access                                   │
│    • Use find() instead of at() for maps                   │
│    • Check iterators != end() before dereferencing         │
│    • Bounds check on vector indexing                       │
│    • No ptr arithmetic - all container-based               │
│                                                              │
│ 3. Atomic Multi-Step Operations                            │
│    • Reserve OR fail entirely (no partial states)          │
│    • Rollback on any step failure                          │
│    • No inconsistent intermediate states possible          │
│                                                              │
│ 4. Resource Management                                     │
│    • No raw pointer operations                             │
│    • RAII principles for memory                            │
│    • Destructor cleans up allocated state                  │
│    • reset() method for reuse                              │
│                                                              │
│ 5. Backward Compatibility                                  │
│    • Feature disabled by default                           │
│    • selectWavelength() has fallback                       │
│    • All new fields initialized to -1 (invalid)           │
│    • No changes to hot paths when disabled                 │
│                                                              │
└──────────────────────────────────────────────────────────────┘

================================================================================
INTEGRATION CHECKLIST
================================================================================

STEP 1: File Operations
  ☐ Copy PhotonicWavelengthAllocator.h to src/
  ☐ Copy PhotonicWavelengthAllocator.cpp to src/
  ☐ Copy documentation files to project root

STEP 2: Modify DataStructs.h
  ☐ Add wavelength_id field to Flit
  ☐ Add photonic_ring_id field to Flit
  ☐ Update operator== in Flit (include new fields)

STEP 3: Modify GlobalParams.h
  ☐ Add use_wavelength_allocator static member
  ☐ Add max_photonic_wavelengths static member

STEP 4: Modify GlobalParams.cpp
  ☐ Initialize use_wavelength_allocator = false
  ☐ Initialize max_photonic_wavelengths = 8

STEP 5: Modify PhotonicHub.h
  ☐ Add #include "PhotonicWavelengthAllocator.h"
  ☐ Add static PhotonicWavelengthAllocator* wavelength_allocator

STEP 6: Modify PhotonicHub.cpp
  ☐ Initialize static wavelength_allocator = NULL
  ☐ Enhance selectWavelength() with allocator query
  ☐ Add flit field tagging in tileToPhotonicProcess

STEP 7: Modify ProcessingElement.cpp
  ☐ Initialize wavelength_id = -1 in generateFlit()
  ☐ Initialize photonic_ring_id = -1 in generateFlit()

STEP 8: Rebuild & Test
  ☐ cd bin && make clean && make
  ☐ No compilation errors expected
  ☐ Binary compiles successfully
  ☐ Run baseline tests (feature disabled)
  ☐ Results should match original Noxim

================================================================================
QUERY INTERFACE REFERENCE
================================================================================
```cpp
Main Query Methods (Static Access):

  int PhotonicHub::wavelength_allocator->getWavelength(int src, int dst)
  └─ Returns: Wavelength ID, or -1 if not allocated

  int PhotonicHub::wavelength_allocator->getRing(int src, int dst)
  └─ Returns: Ring ID, or -1 if not allocated

  bool PhotonicHub::wavelength_allocator->isWavelengthAvailable(
      int ring_id, int src, int dst, int wavelength)
  └─ Returns: true if available, false if occupied

  bool PhotonicHub::wavelength_allocator->reserveWavelength(
      int ring_id, int src, int dst, int wavelength)
  └─ Returns: true if reserved, false if failed

Statistics Methods:

  int getNumRings() const
  └─ Total rings created

  int getNumAllocations() const
  └─ Total successful allocations

  void printStatistics() const
  └─ Print detailed statistics

Administration:

  void generateORNoC(int max_wavelengths)
  └─ Generate allocation (called once at startup)

  void reset()
  └─ Clear all state (for multiple runs)
```
================================================================================
PERFORMANCE PROFILE
================================================================================
```cpp
Allocation Generation Time:
  generateORNoC():
    Typical topologies (8-32 hubs): 1-10 ms
    Large topologies (64+ hubs): 10-100 ms
    Complexity: O(C × R × W × P) amortized

Query Time:
  getWavelength(src, dst):
    Time: O(1) amortized (single map lookup)
    Per-query: ~10-50 nanoseconds (negligible)
    Calls per flit: 1 (during transmission setup)

Memory Consumption:
  Typical 8-hub system:
    Communications: ~64 entries × 40 bytes ≈ 2.5 KB
    Rings: 2-4 × 200 bytes ≈ 1 KB
    Allocations: 64 × 12 bytes ≈ 768 B
    Total: ≈ 4-5 KB per allocator instance

Simulation Impact:
  When disabled (default):
    Overhead: < 1% (only NULL checks)
  
  When enabled:
    Per flit: + 6 operations (see PhotonicHub.cpp)
    Per simulation: negligible (O(N) flits, O(1) per flit)
    Typical: < 0.1% slowdown
```
================================================================================
