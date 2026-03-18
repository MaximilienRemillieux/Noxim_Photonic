#ifndef __NOXIMSELECTION_BUFFER_LEVEL_H__
#define __NOXIMSELECTION_BUFFER_LEVEL_H__

#include "SelectionStrategy.h"
#include "SelectionStrategies.h"
#include "../Router.h"

using namespace std;

class Selection_BUFFER_LEVEL_VC : SelectionStrategy {
	public:
        int apply(Router * router, const vector < int >&directions, const RouteData & route_data);
        void perCycleUpdate(Router * router);

		static Selection_BUFFER_LEVEL_VC * getInstance();

	private:
		Selection_BUFFER_LEVEL_VC(){};
		~Selection_BUFFER_LEVEL_VC(){};

		static Selection_BUFFER_LEVEL_VC * selection_BUFFER_LEVEL_VC;
		static SelectionStrategiesRegister selectionStrategiesRegister;
};

#endif
