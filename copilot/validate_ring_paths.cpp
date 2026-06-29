/*
 * Ring Path Validation Test
 * Tests the physical ring topology implementation
 */

#include <iostream>
#include <vector>
#include <map>
#include <utility>
#include <iomanip>

using namespace std;

// ==================================================================================
// Ring Topology Definition (from PhotonicWavelengthAllocator)
// ==================================================================================

const vector<int> PHYSICAL_RING_SEQUENCE = {
    0, 1, 2, 3, 7, 6, 5, 9, 10, 11, 15, 14, 13, 12, 8, 4
};

// ==================================================================================
// Path Computation Function (mimics PhotonicWavelengthAllocator::computePhysicalRingPath)
// ==================================================================================

vector<pair<int,int>> computePhysicalRingPath(int src, int dst) {
    vector<pair<int,int>> portions;
    
    // Build position map
    map<int, int> ring_position;
    for (size_t i = 0; i < PHYSICAL_RING_SEQUENCE.size(); ++i) {
        ring_position[PHYSICAL_RING_SEQUENCE[i]] = i;
    }
    
    // Validate that source and destination are in the ring
    if (ring_position.find(src) == ring_position.end() ||
        ring_position.find(dst) == ring_position.end()) {
        cerr << "ERROR: Hub not found in physical ring sequence" << endl;
        portions.push_back(make_pair(src, dst));
        return portions;
    }
    
    // Get positions in the ring
    int pos_src = ring_position[src];
    int pos_dst = ring_position[dst];
    int ring_size = PHYSICAL_RING_SEQUENCE.size();
    
    // Generate all portions following the ring unidirectionally
    int current_pos = pos_src;
    
    while (current_pos != pos_dst) {
        int current_hub = PHYSICAL_RING_SEQUENCE[current_pos];
        int next_pos = (current_pos + 1) % ring_size;
        int next_hub = PHYSICAL_RING_SEQUENCE[next_pos];
        
        // Add portion from current to next
        portions.push_back(make_pair(current_hub, next_hub));
        
        current_pos = next_pos;
    }
    
    return portions;
}

// ==================================================================================
// Main Validation Test
// ==================================================================================

int main() {
    cout << "\n";
    cout << "================================================================================\n";
    cout << "  Physical Ring Topology Validation Test\n";
    cout << "================================================================================\n\n";
    
    // Display ring topology
    cout << "Physical Ring Sequence (4x4 Grid):\n";
    cout << "  Layout:\n";
    cout << "    0   1   2   3\n";
    cout << "    4   5   6   7\n";
    cout << "    8   9  10  11\n";
    cout << "   12  13  14  15\n\n";
    cout << "  Ring (unidirectional, no crossings):\n";
    cout << "    ";
    for (size_t i = 0; i < PHYSICAL_RING_SEQUENCE.size(); ++i) {
        cout << PHYSICAL_RING_SEQUENCE[i];
        if (i < PHYSICAL_RING_SEQUENCE.size() - 1) cout << " -> ";
    }
    cout << " -> 0 (wraps around)\n\n";
    
    // Test cases
    struct TestCase {
        int src, dst;
        string description;
    };
    
    vector<TestCase> test_cases = {
        {0, 7, "Forward path in first row and down"},
        {13, 4, "Backward wrap-around path"},
        {15, 1, "Long wrap-around path"},
        {6, 12, "Multi-hop path with wrap"}
    };
    
    for (size_t t = 0; t < test_cases.size(); ++t) {
        int src = test_cases[t].src;
        int dst = test_cases[t].dst;
        
        cout << "================================================================================\n";
        cout << "Test Case " << (t + 1) << ": " << src << " -> " << dst << "\n";
        cout << "Description: " << test_cases[t].description << "\n";
        cout << "================================================================================\n\n";
        
        // Compute path
        vector<pair<int,int>> path = computePhysicalRingPath(src, dst);
        
        // Display path
        cout << "Computed Path Portions:\n";
        cout << "  ";
        for (size_t i = 0; i < path.size(); ++i) {
            cout << "(" << path[i].first << "," << path[i].second << ")";
            if (i < path.size() - 1) cout << " ";
        }
        cout << "\n\n";
        
        // Display hop-by-hop traversal
        cout << "Hop-by-Hop Traversal:\n";
        cout << "  " << src;
        for (size_t i = 0; i < path.size(); ++i) {
            cout << " -> " << path[i].second;
        }
        cout << "\n\n";
        
        // Calculate distance (number of hops)
        cout << "Path Metrics:\n";
        cout << "  Number of portions: " << path.size() << "\n";
        cout << "  Number of hops: " << path.size() << "\n";
        
        // Find position of src and dst in ring
        int pos_src = -1, pos_dst = -1;
        for (size_t i = 0; i < PHYSICAL_RING_SEQUENCE.size(); ++i) {
            if (PHYSICAL_RING_SEQUENCE[i] == src) pos_src = i;
            if (PHYSICAL_RING_SEQUENCE[i] == dst) pos_dst = i;
        }
        
        // Calculate ring distance (unidirectional)
        int distance = (pos_dst - pos_src + PHYSICAL_RING_SEQUENCE.size()) % PHYSICAL_RING_SEQUENCE.size();
        cout << "  Ring distance (unidirectional): " << distance << " hops\n";
        cout << "\n";
    }
    
    cout << "================================================================================\n";
    cout << "  Validation Test Complete\n";
    cout << "================================================================================\n\n";
    
    return 0;
}
