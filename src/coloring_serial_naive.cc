#include "coloring_base.h"

// Naive coloring implementation
template <class vertex>
void Compute(graph<vertex> &GA, commandLine P)
{
    // Check that graph is undirected (out degree == in degree for all vertices)
    ensureUndirected(GA);
    
    const size_t numVertices = GA.n;
    Color* colorData = new Color[numVertices];    
    const uintT maxDegree = getMaxDeg(GA);

    // Verbose variables
    bool verbose = true;
    uintT activeVertices;
    uintT activeEdges;
    timer fullTimer, iterTimer;

    // Make new scheduler and schedule all vertices
    BitsetScheduler currentSchedule(numVertices);
    currentSchedule.reset();
    currentSchedule.scheduleAll(false);

    fullTimer.start();
    double lastStopTime = iterTimer.getTime();
    
    // Loop over vertices until nothing is scheduled
    uint64_t iter = 0;
    while (true)
    {
        iter++;
        // Check if schedule is empty and break out of loop if it is
        if (currentSchedule.anyScheduledTasks() == false)
        {
            break;
        }

        if (verbose)
        {
            std::cout << std::endl;
            std::cout << "Iteration: " << iter << std::endl;
        }
        activeVertices = 0;
        activeEdges = 0;

        currentSchedule.newIteration();
        activeVertices = currentSchedule.numTasks();

        // Parallel loop where each vertex is assigned a color
        for(uintT v_i = 0; v_i < numVertices; v_i++)
        {
            if (currentSchedule.isScheduled(v_i))
            {
                // Get current vertex's neighbours
                const uintT vDegree = GA.V[v_i].getOutDegree();
                bool scheduleNeighbors = false;
                
                activeEdges += vDegree;

                // Make bool array for possible color values and then set any color
                // already taken by neighbours to false
                std::vector<bool> possibleColors(maxDegree + 1, true);
                
                for (uintT n_i = 0; n_i < vDegree; n_i++)
                {
                    uintT neigh = GA.V[v_i].getOutNeighbor(n_i);
                    uintT neighVal = colorData[neigh].color;
                    possibleColors[neighVal] = false;  
                }

                // Find minimum color by iterating through color array in increasing order
                uintT newColor = 0;
                uintT currentColor = colorData[v_i].color; 
                while (newColor <= vDegree)
                {                    
                    // If color is available and it is not the vertex's current value then try to assign
                    if (possibleColors[newColor])
                    {
                        if (currentColor != newColor)
                        {
                            colorData[v_i].color = newColor;
                            scheduleNeighbors = true;
                        }

                        break;
                    }
                    newColor++;
                }

                // Schedule all neighbours if required
                if (scheduleNeighbors)
                {
                    for(uintT n_i = 0; n_i < vDegree; n_i++)
                    {
                        uintT neigh = GA.V[v_i].getOutNeighbor(n_i);
                        // if (neighColors[n_i] > newColor)
                        currentSchedule.schedule(neigh, false);
                    }
                }
            }
        }
        if (verbose)
        {
            std::cout << "\tActive Vs: " << activeVertices << std::endl;
            std::cout << "\tActive Es: " << activeEdges << std::endl;
            std::cout << "\tTime: " << setprecision(TIME_PRECISION) << iterTimer.getTime() - lastStopTime << std::endl;
            lastStopTime = iterTimer.getTime();
        }
    }
    if (verbose)
    {
        cout << "\nTotal Time : " << setprecision(TIME_PRECISION) << fullTimer.stop() << "\n";
    }

    // Assess graph and cleanup
    assessGraph(GA, colorData, maxDegree);
    delete[] colorData;
}