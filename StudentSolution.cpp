#include "acequia_manager.h"
#include <vector>
using namespace std ;

// Holds info for each region (like how much water it needs or has extra)
struct RegionData
{
    Region* region;
    double surplus;
    double need ;
    bool isDrought;
    bool isFlooded;
};

// Finds the canal index between two regions
int getCanalIndex(int from, int to)
{
    if (from == 0 && to == 1)  // North → South
    	return 0;
    if (from == 1 && to == 2) 
    	return 1 ;
    if (from == 0 && to == 2) 
    	return 2;
    if (from == 2 && to == 0) 
    	return 3;
    return -1;
}

// This function return actual Canal* between two regions or it return nullptr if not possible
Canal* getCanal( int from, int to, vector<Canal*> canals)
{
    int index = getCanalIndex (from, to);
    if (index >= 0 && index < canals.size())
    {
        return canals[index];
    }
    
    return nullptr ;
}

// i calculated urgency dependin upon need, drought or empty space
double computeScore( double need, double cap, double level, bool drought )
{
    double score = need;
    if (drought) 
    	{
    		score = 20.0 + score;
    	}
    double space = cap - level;
    score = 0.1 * space + score ;
    
    return score;
}

// I sorted the vector for score in descending order
void sortDescending(vector<pair<double, int>>& list)
{
    for (int i = 0;  i < list.size(); ++i )
    {
        for (int j = i + 1; j < list.size(); ++j)
        {
            if (list[j].first > list[i].first )
            {
                pair<double, int> temp = list [i];
                list[i] = list[j];
                list[j] = temp ;
            }
        }
    }
}

// already called by simulator
void solveProblems(AcequiaManager& manager)
{
    vector<Canal*> canals = manager.getCanals();
    vector<Region*> regions = manager.getRegions();
    int n = regions.size();
    double maxFlow = 1.0 ;
    double margin = 0.05; // keep 5% of water in donor region for safety

    while (!manager.isSolved && manager.hour < manager.SimulationMax)
    {
        vector<RegionData> data(n);

        for (int i = 0; i < n; ++i)
		{
		    // collect region info
		    Region* r = regions[i];
		    data[i].region = r ;

		    if (r->waterLevel > r->waterNeed)
		    {
		        data[i].surplus = r->waterLevel - r->waterNeed ;
		    }
		    else
		    {
		        data[i].surplus = 0.0 ;
		    }

		    if (r->waterLevel < r->waterNeed)
		    {
		        data[i].need = r->waterNeed - r->waterLevel ;
		    }
		    else
		    {
		        data[i].need = 0.0 ;
		    }

		    data[i].isDrought = r->isInDrought ;
		    data[i].isFlooded = r->isFlooded ;
		}

        // prepare lists of needy and donor regions
        vector<pair<double, int>> needs;
        vector<pair<double, int>> donors ;

        for (int i = 0; i < n; ++i)
        {
            if (data[i].need > 0 && !data[i].isFlooded)
            {
                // calculate priority score for needy region
                double score = computeScore(
                    data[i].need,
                    data[i].region->waterCapacity,
                    data[i].region->waterLevel,
                    data[i].isDrought
                );
                needs.push_back(make_pair(score, i));
            }

            if (data[i].surplus > 0 && !data[i].isDrought)
            {
                // donor with available water and not in drought
                donors.push_back(make_pair(data[i].surplus, i));
            }
        }

        sortDescending(needs);
        sortDescending(donors) ;

        for (int i = 0; i < canals.size(); ++i)
        {
            canals[i]->toggleOpen(false);
        }

        int i = 0, j = 0;

        while (i < needs.size() && j < donors.size())
        {
            int to = needs[i].second ;
            int from = donors[j].second;

            Canal* canal = getCanal(from, to, canals);

            if (canal != nullptr)
            {
                // calculate safe flow amount
                double donorSafe = data[from].surplus - (margin * data[from].region->waterCapacity);
                if (donorSafe < 0) donorSafe = 0 ;

                double amount = donorSafe ;
                if (data[to].need < amount) 
                {
                	amount = data[to].need;
                }
                if (amount > maxFlow) 
                	{
                		amount = maxFlow;
                	}

                if (amount > 0.0)
                {
                    canal->setFlowRate(amount);
                    canal->toggleOpen(true) ;
                    data[from].surplus -= amount;
                    data[to].need -= amount ;

                    if (data[to].need <= 0) ++i;
                    if (data[from].surplus <= 0) ++j;
                }
                else
                {
                    ++j;  // try another donor
                }
            }
            else
            {
                ++j; // no direct canal found
            }
        }

        manager.nexthour() ;
    }
}
