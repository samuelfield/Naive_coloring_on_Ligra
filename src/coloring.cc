#include <iostream>
#include <thread>
#include <mutex>

#include "coloring.hh"
#include "bitsetscheduler.h"
#include "parallel.h"
#include "util.h"
#include "gettime.h"

#define TIME_PRECISION 3


// Naive coloring implementation
// active vertices, and time
void naive_coloring(Graph& graph, bool verbose)
{
    uint32_t numVertices = graph.sizeVertex();
    // Verbose variables
    uint32_t activeVertices;
    uint32_t activeEdges;
    timer fullTimer, iterTimer;
    fullTimer.start();
    double lastStopTime = iterTimer.getTime();

    // Make new scheduler and schedule all vertices
    BitsetScheduler currentSchedule(numVertices);
    currentSchedule.reset();
    currentSchedule.scheduleAll();

    // Loop over vertices until nothing is scheduled
    long iter = 0;
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
        cilk_for(uint32_t v = 0; v < numVertices; v++)
        {
            if (currentSchedule.isScheduled(v))
            {
                // std::cout << "vertex: " << v << std::endl; // Debug
                
                // Get current vertex's neighbours
                std::vector<int> neighbors;
                uint32_t vDegree = graph.getNeighbors(neighbors, v);
                int32_t vMaxValue = (int32_t) vDegree + 1;
                bool scheduleNeighbors = false;
                activeEdges += vDegree;
                
                // Make bool array for possible color values and then set any color
                // already taken by neighbours to false 
                std::vector<bool> possibleValues(vMaxValue, true);
                cilk_for(uint32_t n = 0; n < vDegree; n++)
                {
                    int32_t neighVal = graph.getVertexValue(neighbors[n]);
                    possibleValues[neighVal] = false;
                }

                // std::cout << std::endl; // Debug

                // Find minimum color by iterating through color array in increasing order
                int32_t newValue = 0;
                while (newValue < vMaxValue)
                {
                    // std::cout << "Color: " << newValue << " is " << possibleValues[newValue] << std::endl; // Debug 
                    
                    // If color is available and it is not the vertex's current value then try to assign
                    if (possibleValues[newValue])
                    {
                        if (graph.getVertexValue(v) != newValue)
                        {
                            // std::cout << "Does not equal: " << graph.getVertexValue(v) << " != " << newValue << std::endl; // Debug
                            graph.setVertexValue(v, newValue);
                            scheduleNeighbors = true;
                        }
                        
                        break;
                    }

                    newValue++;
                }

                // Schedule all neighbours if required
                if (scheduleNeighbors)
                {
                    for (uint32_t n = 0; n < vDegree; n++)
                    {
                        currentSchedule.schedule(neighbors[n], false);
                    }
                }

                // std::cout << std::endl; // Debug
            }
        }

        if (verbose)
        {
            std::cout << "\tActive Vs: " << activeVertices << std::endl;
            std::cout << "\tActive Es: " << activeEdges << std::endl;
            std::cout << "\tTime: " << setprecision(TIME_PRECISION) << iterTimer.getTime() - lastStopTime << std::endl;
            lastStopTime = iterTimer.getTime();
        }

        // nanosleep((const struct timespec[]){{0, 500000000L}}, NULL);
    }
    if (verbose)
    {
        cout << "\nTotal Time : " << setprecision(TIME_PRECISION) << fullTimer.stop() << "\n";
    }
}


// Go through every vertex and check that it's color does not conflict with neighbours
// while also checking that each vertex is minimally colored
bool assessGraph(Graph& graph)
{
    uint32_t numVertices = graph.sizeVertex();

    cilk_for(uint32_t v = 0; v < numVertices; v++)
    {
        uint32_t vValue = (uint32_t) graph.getVertexValue(v);
        std::vector<int> neighbors;
        uint32_t vDegree = graph.getNeighbors(neighbors, v);
        std::vector<bool> possibleColors(vDegree + 1, true);
        uint32_t minimalColor = 0;

        cilk_for(uint32_t n = 0; n < vDegree; n++)
        {
            uint32_t neighVal = graph.getVertexValue(neighbors[n]);
            possibleColors[neighVal] = false;
            
            if (neighVal == vValue)
            {
                std::cout << "Failure: Color for vertex " << v << " conflicts with vertex " << n << "!" << std::endl;
                return false;
            }
        }

        while (!possibleColors[minimalColor] && (minimalColor < vDegree + 1))
        {
            minimalColor++;
        }

        if (vValue != minimalColor)
        {
            std::cout << "Failure: Color for vertex " << v << " not minimal!" << std::endl;
            return false;
        }
    }

    std::cout << "Successful Coloring!" << std::endl;
    return true;
}