#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <random>

#include "bitsetscheduler.h"
#include "ligra.h"
#include "gettime.h"

#define TIME_PRECISION 3

// Go through every vertex and check that it's color does not conflict with neighbours
// while also checking that each vertex is minimally colored
template <class vertex>
void assessGraph(graph<vertex> &GA, uint64_t *colorData)
{
    uint32_t numVertices = GA.n;
    uintT conflict = 0;
    uintT notMinimal = 0;

    parallel_for(uintT v_i = 0; v_i < numVertices; v_i++)
    {
        uint64_t vValue = colorData[v_i];  
        uintT vDegree = GA.V[v_i].getOutDegree();
        std::vector<bool> possibleColors(vDegree + 1, true);
        uint32_t minimalColor = 0;

        parallel_for(uintT n_i = 0; n_i < vDegree; n_i++)
        {
            uintT i = GA.V[v_i].getOutNeighbor(n_i);
            uint64_t neighVal = colorData[i];
            possibleColors[neighVal] = false;
            
            if (neighVal == vValue)
            {
                conflict++;
            }
        }

        while (!possibleColors[minimalColor] && (minimalColor < vDegree + 1))
        {
            minimalColor++;
        }

        if (vValue != minimalColor)
        {
            notMinimal++;
        }
    }

    if (conflict != 0)
    {
        std::cout << "Failure: color conflicts on " << conflict << " vertices" << std::endl;
    }
    else if (notMinimal != 0)
    {
        std::cout << "Failure: minimality condition broken for " << notMinimal << " vertices" << std::endl;
    }
    else
    {
        std::cout << "Successful Coloring!" << std::endl;
    }
}

template <class vertex>
void randomizeVertexValues(graph<vertex> &GA, std::vector<uintT> &colorData)
{
    uintT maxDegree = 0;
    for (uintT v_i=0; v_i < GA.n; v_i++)
    {
        if (GA.V[v_i].getOutDegree() > maxDegree)
        {
            maxDegree = GA.V[v_i].getOutDegree();
        }
    }
    std::cout << "Max degree: " << maxDegree << std::endl; 
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<uintT> dist(0, maxDegree);

    for (uintT v_i=0; v_i < GA.n; v_i++)
    {
        colorData[v_i] = dist(mt);
    }
}


// Naive coloring implementation
// active vertices, and time
template <class vertex>
void Compute(graph<vertex> &GA, commandLine P)
{
    uintT numVertices = GA.n;
    std::vector<uintT> colorData(numVertices, 0);
    // randomizeVertexValues(GA, colorData);
    
    std::vector<DenseBitset*> possibleValues(numVertices);
    parallel_for(uintT v_i=0; v_i < numVertices; v_i++)
    {
        possibleValues[v_i] = new DenseBitset(GA.V[v_i].getOutDegree());
    }

    // Verbose variables
    bool verbose = true;
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
        parallel_for(uintT v_i = 0; v_i < numVertices; v_i++)
        {
            if (currentSchedule.isScheduled(v_i))
            {
                // std::cout << "vertex: " << v_i << std::endl; // Debug
                
                // Get current vertex's neighbours
                uintT vDegree = GA.V[v_i].getOutDegree();
                uintT vMaxValue = vDegree + 1;
                bool scheduleNeighbors = false;
                activeEdges += vDegree;
                
                // Make bool array for possible color values and then set any color
                // already taken by neighbours to false 
                
                possibleValues[v_i]->setAll();
                // bool possibleValues[4847571] = { false };
                // std::vector<bool> possibleValues(vMaxValue, true);
                parallel_for(uintT n_i = 0; n_i < vDegree; n_i++)
                {
                    uintT neigh = GA.V[v_i].getOutNeighbor(n_i);
                    uintT neighVal = colorData[neigh];//.color;
                    possibleValues[v_i]->set(neighVal, false);
                    // possibleValues[neighVal] = false; 
                }
                // std::cout << std::endl; // Debug

                // Find minimum color by iterating through color array in increasing order
                uintT newColor = 0;
                uintT currentColor = colorData[v_i];//.color; 
                while (newColor < vMaxValue)
                {                    
                    // If color is available and it is not the vertex's current value then try to assign
                    if (possibleValues[v_i]->get(newColor))
                    {
                        if (currentColor != newColor)
                        {
                            colorData[v_i] = newColor;//.color = newColor;
                            scheduleNeighbors = true;
                        }
                        break;
                    }
                    newColor++;
                }

                // Schedule all neighbours if required
                if (scheduleNeighbors)
                {
                    for (uintT n_i = 0; n_i < vDegree; n_i++)
                    {
                        uintT i = GA.V[v_i].getOutNeighbor(n_i);
                        currentSchedule.schedule(i, false);
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

        // nanosleep((const struct timespec[]){{0, 500000000L}}, NULL);
    }
    if (verbose)
    {
        cout << "\nTotal Time : " << setprecision(TIME_PRECISION) << fullTimer.stop() << "\n";
    }

    parallel_for(uintT v_i=0; v_i < numVertices; v_i++)
    {
        free(possibleValues[v_i]);
    }

    // assessGraph(GA, colorData);
    // free(colorData);
}


