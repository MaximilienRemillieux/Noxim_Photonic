# Optional Enhancement: Load-Balanced Wavelength Allocation

## File: PhotonicWavelengthAllocator.cpp (Optional Enhancement)

### Enhancement #1: Load-Balanced Wavelength Selection

Add this new method to the `PhotonicWavelengthAllocator` class:

```cpp
// ==================================================================================
// OPTIONAL ENHANCEMENT: Load-Balanced Wavelength Allocation
// ==================================================================================

/**
 * @brief Find available wavelength with load balancing
 * 
 * Instead of returning the first available wavelength, analyzes usage
 * across all portions in the ring and prefers less-loaded wavelengths.
 * This distributes communications more evenly across spectrum.
 */
int PhotonicWavelengthAllocator::findAvailableWavelengthBalanced(
    int ring_id, const vector<pair<int,int>>& portions) const {
    
    if (ring_id < 0 || ring_id >= (int)rings.size()) {
        return -1;
    }
    
    // Count how many times each wavelength is used in this ring
    vector<int> wl_usage_count(rings[ring_id].max_wavelengths, 0);
    
    for (map<pair<int,int>, vector<bool>>::const_iterator it = 
         rings[ring_id].occupation.begin(); 
         it != rings[ring_id].occupation.end(); ++it) {
        for (int wl = 0; wl < (int)it->second.size(); ++wl) {
            if (it->second[wl]) {
                wl_usage_count[wl]++;
            }
        }
    }
    
    // Create (usage_count, wavelength_id) pairs and sort by usage
    vector<pair<int,int>> sorted_wavelengths;
    for (int wl = 0; wl < rings[ring_id].max_wavelengths; ++wl) {
        sorted_wavelengths.push_back(make_pair(wl_usage_count[wl], wl));
    }
    sort(sorted_wavelengths.begin(), sorted_wavelengths.end());
    
    // Try wavelengths in order of increasing usage (least-loaded first)
    for (size_t i = 0; i < sorted_wavelengths.size(); ++i) {
        int wl = sorted_wavelengths[i].second;
        
        // Check if this wavelength is available on ALL portions
        bool available_on_all_portions = true;
        for (size_t p = 0; p < portions.size(); ++p) {
            if (!isWavelengthAvailable(ring_id, portions[p].first, 
                                      portions[p].second, wl)) {
                available_on_all_portions = false;
                break;
            }
        }
        
        if (available_on_all_portions) {
            // Debug output
            cout << "ORNoC Debug: Selected wavelength " << wl 
                 << " (usage count: " << wl_usage_count[wl] << ")" << endl;
            return wl;  // Return least-used available wavelength
        }
    }
    
    return -1;  // No wavelength available on this ring
}
```

### To Use This Enhancement

Replace this line in `generateORNoC()`:

```cpp
// Original line in the "try existing rings" loop
int wl = findAvailableWavelength(r, portions);
```

With:

```cpp
// Enhanced version with load balancing
int wl = findAvailableWavelengthBalanced(r, portions);
```

### Performance Analysis

- **Time Complexity:** O(hubs² + hubs × max_wavelengths) per allocation
  - Counting usage: O(hubs²) in worst case
  - Sorting: O(max_wavelengths × log(max_wavelengths))
  - Availability check: O(max_wavelengths)
  - For 8×8 mesh (16 hubs) with 8 wavelengths: very fast (<1ms)

- **Expected Outcome:**
  - Wavelengths 0-7 should be used (not just 0)
  - Load more balanced across spectrum
  - More efficient spectrum utilization

---

## Enhancement #2: Intelligent Ring Creation Limits

Add mechanism to limit rings and force better packing:

```cpp
// In generateORNoC(), modify new ring creation section:

const int MAX_RINGS_PER_ALLOCATION_ATTEMPT = 3;

if (!allocated && rings.size() < MAX_RINGS_PER_ALLOCATION_ATTEMPT) {
    // Create new ring only if ring count is reasonable
    PhotonicRing new_ring(current_ring_id, max_wavelengths, alternate_direction);
    // ... rest of ring creation
    
} else if (!allocated && rings.size() >= MAX_RINGS_PER_ALLOCATION_ATTEMPT) {
    // Too many rings - communication cannot be allocated
    cerr << "ORNoC: WARNING - Communication (" << comm.src << " -> " << comm.dst 
         << ") could not be allocated after " << rings.size() 
         << " rings. Skipping." << endl;
    
    // Mark as processed anyway to avoid infinite loop
    comm.processed = true;
}
```

---

## Enhancement #3: Round-Robin Wavelength Allocation

Simplest alternative to load balancing - uses modulo arithmetic:

```cpp
// Replace findAvailableWavelength() call with:

static int round_robin_counter = 0;
int wl = findAvailableWavelengthRoundRobin(r, portions, round_robin_counter);
if (wl >= 0) {
    round_robin_counter = (wl + 1) % rings[r].max_wavelengths;
}

// New helper method:
int PhotonicWavelengthAllocator::findAvailableWavelengthRoundRobin(
    int ring_id, const vector<pair<int,int>>& portions, int start_wl) const {
    
    if (ring_id < 0 || ring_id >= (int)rings.size()) {
        return -1;
    }
    
    int max_wl = rings[ring_id].max_wavelengths;
    
    // Try starting from round_robin position, wrap around to 0
    for (int attempt = 0; attempt < max_wl; ++attempt) {
        int wl = (start_wl + attempt) % max_wl;
        
        bool available_on_all_portions = true;
        for (size_t p = 0; p < portions.size(); ++p) {
            if (!isWavelengthAvailable(ring_id, portions[p].first, 
                                      portions[p].second, wl)) {
                available_on_all_portions = false;
                break;
            }
        }
        
        if (available_on_all_portions) {
            return wl;
        }
    }
    
    return -1;
}
```

**Advantage:** Simple, no sorting overhead, fair distribution

---

## Comparison of Approaches

| Approach | Complexity | Distribution | Fairness | Memory |
|----------|-----------|--------------|----------|--------|
| Current (greedy) | O(hubs×wl) | All wavelength 0 | ❌ Bad | Min |
| Pre-init fix | O(hubs×wl) | Mixed (with conflicts) | ⚠️ OK | O(hubs²) |
| Load-balanced | O(hubs²+hubs×wl) | Even across wl | ✅ Good | O(hubs²) |
| Round-robin | O(hubs×wl) | Sequential (0,1,2...) | ✅ Good | O(hubs²) |

---

## Recommended Strategy

1. **Phase 1:** Apply pre-initialization fix **[DONE]**
   - Quick, low-risk, fixes core issue
   - Test with existing test suite

2. **Phase 2 (if needed):** Apply load balancing
   - If distribution still poor after Phase 1
   - Better utilization of spectrum

3. **Phase 3 (if needed):** Ring creation limits
   - If ring count explodes
   - Forces more efficient packing

---

## Testing the Enhancement

```cpp
// Add to printStatistics() method:

cout << "\nWavelength Usage Distribution:" << endl;
int wl_usage[16] = {0};  // Assume max 16 wavelengths

for (int r = 0; r < (int)rings.size(); ++r) {
    for (map<pair<int,int>, vector<bool>>::iterator it = 
         rings[r].occupation.begin(); 
         it != rings[r].occupation.end(); ++it) {
        for (int wl = 0; wl < (int)it->second.size(); ++wl) {
            if (it->second[wl]) {
                wl_usage[wl]++;
            }
        }
    }
}

for (int wl = 0; wl < GlobalParams::max_photonic_wavelengths; ++wl) {
    cout << "  Wavelength " << setw(2) << wl << ": " 
         << setw(3) << wl_usage[wl] << " uses" << endl;
}
```

Expected output AFTER fix:
```
Wavelength Usage Distribution:
  Wavelength  0: 20 uses
  Wavelength  1: 18 uses
  Wavelength  2: 17 uses
  Wavelength  3: 19 uses
  ...
```

Not:
```
Wavelength  0: 120 uses   ❌ Before fix
Wavelength  1: 0 uses
Wavelength  2: 0 uses
...
```
