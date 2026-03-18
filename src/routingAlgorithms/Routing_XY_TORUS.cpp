#include "Routing_XY_TORUS.h"

RoutingAlgorithmsRegister Routing_XY_TORUS::routingAlgorithmsRegister("XY_TORUS", getInstance());

Routing_XY_TORUS * Routing_XY_TORUS::routing_XY_TORUS = 0;

Routing_XY_TORUS * Routing_XY_TORUS::getInstance() {
	if ( routing_XY_TORUS == 0 )
		routing_XY_TORUS = new Routing_XY_TORUS();
    
	return routing_XY_TORUS;
}

vector<int> Routing_XY_TORUS::route(Router * router, const RouteData & routeData)
{
    Coord current = id2Coord(routeData.current_id);
    Coord destination = id2Coord(routeData.dst_id);
    vector <int> directions;

    int dimX = GlobalParams::mesh_dim_x;
    int dimY = GlobalParams::mesh_dim_y;

    int dx = destination.x - current.x;
    int dy = destination.y - current.y;

    // wrap-around for torus
    if (dx >  dimX/2) dx -= dimX;
    if (dx < -dimX/2) dx += dimX;

    if (dy >  dimY/2) dy -= dimY;
    if (dy < -dimY/2) dy += dimY;

    // XY routing order
    if (dx > 0){
        directions.push_back(DIRECTION_EAST);}
    else if (dx < 0){
        directions.push_back(DIRECTION_WEST);}
    else if (dy > 0){
        directions.push_back(DIRECTION_SOUTH);}
    else if (dy < 0){
        directions.push_back(DIRECTION_NORTH);}
    else {
        directions.push_back(DIRECTION_LOCAL);}
    return directions;
   }
