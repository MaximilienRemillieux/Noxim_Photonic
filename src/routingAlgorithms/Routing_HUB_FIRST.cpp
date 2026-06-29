#include "Routing_HUB_FIRST.h"
#include "../GlobalParams.h"

RoutingAlgorithmsRegister Routing_HUB_FIRST::routingAlgorithmsRegister("HUB_FIRST", getInstance());

Routing_HUB_FIRST * Routing_HUB_FIRST::routing_HUB_FIRST = 0;

Routing_HUB_FIRST * Routing_HUB_FIRST::getInstance() {
    if (routing_HUB_FIRST == 0)
        routing_HUB_FIRST = new Routing_HUB_FIRST();
    return routing_HUB_FIRST;
}

////////////////////////////////////////////////////////////

int Routing_HUB_FIRST::manhattanDistance(int src_id, int dst_id) {
    Coord src = id2Coord(src_id);
    Coord dst = id2Coord(dst_id);
    return abs(src.x - dst.x) + abs(src.y - dst.y);
}

// Helper returning the ID of the nearest attached tile (photonic hub entry point)
int Routing_HUB_FIRST::nearestHubNode(int node_id) {
    int best_node = -1;
    int min_dist = 1e9;
    for (auto &entry : GlobalParams::photonic_hub_configuration) {
        const PhotonicHubConfig &hc = entry.second;
        for (int hub_node : hc.attachedNodesPhotonic) {
            if (hub_node == node_id) continue;  // Skip if it's the node itself
            int d = manhattanDistance(node_id, hub_node);
            if (d < min_dist) {
                min_dist = d;
                best_node = hub_node;
            }
        }
    }
    return best_node;
}

////////////////////////////////////////////////////////////

// Helper returning the distance to the nearest photonic hub (used for route decision)
int Routing_HUB_FIRST::nearestHubDistance(int node_id) {
    int min_dist = 1e9;
    for (auto &entry : GlobalParams::photonic_hub_configuration) {
        const PhotonicHubConfig &hc = entry.second;
        for (int hub_node : hc.attachedNodesPhotonic) {
            int d = manhattanDistance(node_id, hub_node);
            if (d < min_dist)
                min_dist = d;
        }
    }
    return min_dist;
}

vector<int> Routing_HUB_FIRST::route(Router * router, const RouteData & routeData)
{
    int current = routeData.current_id;
    int destination = routeData.dst_id;
    int vc = routeData.vc_id;
    Coord curr = id2Coord(current);
    Coord dst  = id2Coord(destination);
    vector<int> directions;

    int xy_hops = manhattanDistance(current, destination);
    int dist_to_nearest_hub = nearestHubDistance(current);
    int dist_from_hub_to_dst = nearestHubDistance(destination);
    int hub_path_hops = dist_to_nearest_hub + 1 + dist_from_hub_to_dst;
    bool use_hub = (hub_path_hops < xy_hops);

    if (use_hub == false) { // No Photonichub-assisted path is beneficial, use direct XY routing
        return RoutingAlgorithms::get("XY")->route(router, routeData);
    }
    else { // Photonichub-assisted path is beneficial, try to use it.
           // At a tile attached to a photonic hub, we can inject into the photonic network.
        if (GlobalParams::photonic_hub_for_tile.find(current) != GlobalParams::photonic_hub_for_tile.end()) {
            directions.push_back(DIRECTION_PHOTONIC_HUB);
            return directions;
        }
        // Move toward nearest hub but only if neighbor strictly reduces Manhattan distance
        int hub_node = nearestHubNode(current);
        if (hub_node >= 0) {
            Coord hubc = id2Coord(hub_node);
            if (hubc.x > curr.x){
                directions.push_back(DIRECTION_EAST);
            }
            else if (hubc.x < curr.x){
                directions.push_back(DIRECTION_WEST);
            }
            else if (hubc.y > curr.y){
                directions.push_back(DIRECTION_SOUTH);
            }
            else if (hubc.y < curr.y){
                directions.push_back(DIRECTION_NORTH);
            }
            return directions;
        }
    }
    return directions;
}


////////////////////////////////////////////////////////////

//vector<int> Routing_HUB_FIRST::route(Router * router, const RouteData & routeData) 
//{
//
//    int current = routeData.current_id;
//    int destination = routeData.dst_id;
//    int vc = routeData.vc_id;
//
//    Coord curr = id2Coord(current);
//    Coord dst  = id2Coord(destination);
//    vector<int> directions;
//    // Require at least 2 VCs (VC0: mesh->hub, VC1: hub->mesh)
//     if (GlobalParams::n_virtual_channels < 2) {
//         //LOG << "Routing_HUB_FIRST requires at least 2 virtual channels" << endl;
//         assert(GlobalParams::n_virtual_channels >= 2);
//    }
//
//    //Phase: FROM_HUB (VC1) -> strict XY progress to destination
//    if (vc == 1) {
//        if (dst.x > curr.x){
//            directions.push_back(DIRECTION_EAST);
//            cout << "Current node " << current << " is using XY only: mouving to "
//            << destination << " to final destination VC = " << vc << endl;
//        }
//        else if (dst.x < curr.x){
//            directions.push_back(DIRECTION_WEST);
//            cout << "Current node " << current << " is using XY only: mouving to "
//            << destination << " to final destination VC = " << vc << endl;
//        }
//        else if (dst.y > curr.y){
//            directions.push_back(DIRECTION_SOUTH);
//            cout << "Current node " << current << " is using XY only: mouving to "
//            << destination << " to final destination VC = " << vc << endl;
//        }
//        else if (dst.y < curr.y){
//            directions.push_back(DIRECTION_NORTH);
//            cout << "Current node " << current << " is using XY only: mouving to "
//            << destination << " to final destination VC = " << vc << endl;
//        }
//        else {
//            directions.push_back(DIRECTION_LOCAL);
//            cout << "Current node " << current << " is using XY only: mouving to "
//            << destination << " to final destination VC = " << vc << endl;
//        }
//        return directions;
//    }
//    // Treat any non-VC1 as VC0 (class 0: mesh -> nearest hub)
//    // Decide whether hub-assisted path is beneficial
//    int xy_hops = manhattanDistance(current, destination);
//    int dist_to_nearest_hub = nearestHubDistance(current);
//    int dist_from_hub_to_dst = nearestHubDistance(destination);
//    int hub_path_hops = dist_to_nearest_hub + 1 + dist_from_hub_to_dst;
//    bool use_hub = (hub_path_hops < xy_hops);
//
//    if (use_hub) {
//        bool currHasHub = (GlobalParams::photonic_hub_for_tile.find(current) != GlobalParams::photonic_hub_for_tile.end());
//        bool dstHasHub  = (GlobalParams::photonic_hub_for_tile.find(destination) != GlobalParams::photonic_hub_for_tile.end());
//
//        // At a tile attached to a photonic hub, we can inject into the photonic network.
//        // Two cases:
//        // 1. Destination itself is photonic hub-attached: we just send to the hub port.
//        // 2. Destination is not photonic hub-attached: we must select a relay node
//        //    (some tile connected to the photonic hub closest to the destination) so that
//        //    the hub knows where to forward the flit.
//        if (currHasHub) {
//            if (dstHasHub) {
//                // common case: both ends are photonic hub connected
//                directions.push_back(DIRECTION_PHOTONIC_HUB);
//                cout << "Current node " << current << " is using photonic hub_entry "
//                << GlobalParams::photonic_hub_for_tile.find(current)->first << " to destination photonic hub_exit "
//                << GlobalParams::photonic_hub_for_tile.find(destination)->first << " toward final destination "
//                << destination << " VC = " << vc
//                << endl;
//
//                return directions;
//            } else {
//                // compute a relay node near the destination
//                int relay = nearestHubNode(destination);
//                if (relay >= 0 && GlobalParams::photonic_hub_for_tile.find(relay) != GlobalParams::photonic_hub_for_tile.end()) {
//                    // encode as DIRECTION_HUB_RELAY + tile_id so the router
//                    // will record flit.hub_relay_node.
//                    directions.push_back(DIRECTION_PHOTONIC_HUB_RELAY + relay);
//                    cout << "Current node " << current << " is using photonic hub_relay_entry "
//                    << GlobalParams::photonic_hub_for_tile.find(current)->first << " to destination hub_relay_exit "
//                    << relay << " toward final destination "
//                    << destination << " VC = " << vc
//                    << endl;
//                    return directions;
//                }
//                // if no relay could be found we simply fall through to the
//                // normal mesh movement below rather than sending to hub.
//            }
//        }
//
//        // Move toward nearest hub but only if neighbor strictly reduces Manhattan distance
//        int hub_node = nearestHubNode(current);
//        if (hub_node >= 0) {
//            Coord hubc = id2Coord(hub_node);
//            int currDist = abs(curr.x - hubc.x) + abs(curr.y - hubc.y);
//
//            if (curr.x + 1 < (int)GlobalParams::mesh_dim_x) {
//                int nd = abs((curr.x+1) - hubc.x) + abs(curr.y - hubc.y);
//                if (nd < currDist) {
//                    directions.push_back(DIRECTION_EAST); 
//                    cout << "Current node " << current << " is routing through hub_entry node "
//                    << hub_node << " toward destination "
//                    << destination << " VC = " << vc
//                    << endl;
//                    return directions;
//                }
//            }
//            if (curr.x - 1 >= 0) {
//                int nd = abs((curr.x-1) - hubc.x) + abs(curr.y - hubc.y);
//                if (nd < currDist) {
//                    directions.push_back(DIRECTION_WEST); 
//                    cout << "Current node " << current << " is routing through hub_entry node "
//                    << hub_node << " toward destination "
//                    << destination << " VC = " << vc
//                    << endl;
//                    return directions;
//                }
//            }
//            if (curr.y + 1 < (int)GlobalParams::mesh_dim_y) {
//                int nd = abs(curr.x - hubc.x) + abs((curr.y+1) - hubc.y);
//                if (nd < currDist) {
//                    directions.push_back(DIRECTION_SOUTH);
//                    cout << "Current node " << current << " is routing through hub_entry node "
//                    << hub_node << " toward destination "
//                    << destination << " VC = " << vc
//                    << endl;
//                    return directions;
//                }
//            }
//            if (curr.y - 1 >= 0) {
//                int nd = abs(curr.x - hubc.x) + abs((curr.y-1) - hubc.y);
//                if (nd < currDist) {
//                    directions.push_back(DIRECTION_NORTH);
//                    cout << "Current node " << current << " is routing through hub_entry node "
//                    << hub_node << " toward destination "
//                    << destination << " VC = " << vc
//                    << endl;
//                    return directions;
//                }
//            }
//        }
//
//        // Fallback: progress toward destination using XY (remain in VC0)
//        // This MUST always return at least one valid direction to avoid immediate deadlock
//    }
//
//    // No hub path: standard XY (remain in VC0)
//    // MUST always return at least one valid direction to avoid deadlock
//    if (dst.x > curr.x && curr.x + 1 < (int)GlobalParams::mesh_dim_x) {
//        directions.push_back(DIRECTION_EAST);
//                cout << "Current node " << current << " is using XY only: mouving to "
//                << directions.back() << " to final destination  " 
//                << destination << " VC = " << vc
//                << endl;
//        return directions;
//    }
//    if (dst.x < curr.x && curr.x - 1 >= 0) {
//        directions.push_back(DIRECTION_WEST);
//                cout << "Current node " << current << " is using XY only: mouving to "
//                << directions.back() << " to final destination  " 
//                << destination << " VC = " << vc
//                << endl;
//        return directions;
//    }
//    if (dst.y > curr.y && curr.y + 1 < (int)GlobalParams::mesh_dim_y) {
//        directions.push_back(DIRECTION_SOUTH);
//                cout << "Current node " << current << " is using XY only: mouving to "
//                << directions.back() << " to final destination  " 
//                << destination << " VC = " << vc
//                << endl;
//        return directions;
//    }
//    if (dst.y < curr.y && curr.y - 1 >= 0) {
//        directions.push_back(DIRECTION_NORTH);
//                cout << "Current node " << current << " is using XY only: mouving to "
//                << directions.back() << " to final destination  " 
//                << destination << " VC = " << vc
//                << endl;
//        return directions;
//    }
//    if (curr == dst) {
//        directions.push_back(DIRECTION_LOCAL);
//        return directions;
//    }
//    
//    // Last resort: if completely blocked (all directions hit mesh boundary),
//    // pick any valid neighbor direction to allow forward progress
//    if (curr.x + 1 < (int)GlobalParams::mesh_dim_x) {
//        directions.push_back(DIRECTION_EAST);
//        cout << "Current node " << current << " is using fallback routing to "
//        << directions.back() << " to final destination  "
//        << destination << " VC = " << vc
//        << endl;
//        return directions;
//    }
//    if (curr.x - 1 >= 0) {
//        directions.push_back(DIRECTION_WEST);
//        cout << "Current node " << current << " is using fallback routing to "
//        << directions.back() << " to final destination  "
//        << destination << " VC = " << vc
//        << endl;
//        return directions;
//    }
//    if (curr.y + 1 < (int)GlobalParams::mesh_dim_y) {
//        directions.push_back(DIRECTION_SOUTH);
//        cout << "Current node " << current << " is using fallback routing to "
//        << directions.back() << " to final destination  "
//        << destination << " VC = " << vc
//        << endl;
//        return directions;
//    }
//    if (curr.y - 1 >= 0) {
//        directions.push_back(DIRECTION_NORTH);
//        cout << "Current node " << current << " is using fallback routing to "
//        << directions.back() << " to final destination  "
//        << destination << " VC = " << vc
//        << endl;
//        return directions;
//    }
//    
//    // This should NEVER occur in a valid mesh
//    assert(false && "No valid direction found in fallback routing");
//    return directions;
//}
//