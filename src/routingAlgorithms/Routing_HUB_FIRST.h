#ifndef __NOXIMROUTING_HUB_FIRST_H__
#define __NOXIMROUTING_HUB_FIRST_H__

#include "RoutingAlgorithm.h"
#include "RoutingAlgorithms.h"
#include "../Router.h"

using namespace std;

class Routing_HUB_FIRST : public RoutingAlgorithm {
public:
    vector<int> route(Router * router, const RouteData & routeData);

    static Routing_HUB_FIRST * getInstance();

private:
    Routing_HUB_FIRST(){};
    ~Routing_HUB_FIRST(){};

    static Routing_HUB_FIRST * routing_HUB_FIRST;
    static RoutingAlgorithmsRegister routingAlgorithmsRegister;

    int manhattanDistance(int src_id, int dst_id);
    int nearestHubDistance(int node_id);
    int nearestHubNode(int node_id);
};

#endif
