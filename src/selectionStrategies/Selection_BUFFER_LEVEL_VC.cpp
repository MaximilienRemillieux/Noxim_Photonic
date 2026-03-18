#include "Selection_BUFFER_LEVEL_VC.h"

SelectionStrategiesRegister Selection_BUFFER_LEVEL_VC::selectionStrategiesRegister("BUFFER_LEVEL_VC", getInstance());

Selection_BUFFER_LEVEL_VC * Selection_BUFFER_LEVEL_VC::selection_BUFFER_LEVEL_VC = 0;

Selection_BUFFER_LEVEL_VC * Selection_BUFFER_LEVEL_VC::getInstance() {
	if ( selection_BUFFER_LEVEL_VC == 0 )
		selection_BUFFER_LEVEL_VC = new Selection_BUFFER_LEVEL_VC();
    
	return selection_BUFFER_LEVEL_VC;
}

int Selection_BUFFER_LEVEL_VC::apply(Router * router, const vector < int >&directions, const RouteData & route_data){
    vector < int >best_dirs;
    int max_free_slots = 0;
    for (unsigned int i = 0; i < directions.size(); i++) {

	bool available = false;

	int free_slots = router->free_slots_neighbor[directions[i]].read();

	try {
	    available = router->reservation_table.isNotReserved(directions[i]);
	}
	catch (int error)
	{
	    if (error==NOT_VALID) continue;
	    assert(false);
	}


	if (available) {
	    if (free_slots > max_free_slots) {
		max_free_slots = free_slots;
		best_dirs.clear();
		best_dirs.push_back(directions[i]);
	    } else if (free_slots == max_free_slots)
		best_dirs.push_back(directions[i]);
	}
    }

    if (best_dirs.size())
	return (best_dirs[rand() % best_dirs.size()]);
    else
	return (directions[rand() % directions.size()]);
}

void Selection_BUFFER_LEVEL_VC::perCycleUpdate(Router * router) {
	    // update current input buffers level to neighbors
	    for (int i = 0; i < DIRECTIONS + 1; i++){
		    int total_free = 0;
            for (int vc = 0; vc < GlobalParams::n_virtual_channels; vc++)
                total_free += router->buffer[i][vc].getCurrentFreeSlots();
            router->free_slots[i].write(total_free);
        }
	    // NoP selection: send neighbor info to each direction 'i'
	    NoP_data current_NoP_data = router->getCurrentNoPData();

	    for (int i = 0; i < DIRECTIONS; i++)
		router->NoP_data_out[i].write(current_NoP_data);
}