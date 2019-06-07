#include <iostream>
#include <thread>
#include <mutex>
#include <vector>

#include "bitsetscheduler.h"
#include "ligra.h"
#include "gettime.h"

#define TIME_PRECISION 3

#define CILK

struct Color
{
    uint64_t color;

    Color() : color(0){}

    inline bool operator==(const Color &rhs)
    {
        if (color == rhs.color)
            return true;
        else
            return false;
    }
    inline bool operator!=(const Color &rhs)
    {
        if (color != rhs.color)
            return true;
        else
            return false;
    }
};


// Go through every vertex and check that it's color does not conflict with neighbours
// while also checking that each vertex is minimally colored
template <class vertex>
void assessGraph(graph<vertex> &GA, Color *colorData)
{
    uint32_t numVertices = GA.n;
    uintT conflict = 0;
    uintT notMinimal = 0;

    parallel_for(intT v = 0; v < numVertices; v++)
    {
        uint64_t vValue = colorData[v].color;  
        uintT vDegree = GA.V[v].getOutDegree();
        std::vector<bool> possibleColors(vDegree + 1, true);
        uint32_t minimalColor = 0;

        parallel_for(intT n = 0; n < vDegree; n++)
        {
            intT i = GA.V[v].getOutNeighbor(n);
            uint64_t neighVal = colorData[i].color;
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



// Naive coloring implementation
// active vertices, and time
template <class vertex>
void Compute(graph<vertex> &GA, commandLine P)
{
    intT numVertices = GA.n;
    std::vector<Color> colorData(numVertices);

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
        cilk_for(intT v = 0; v < numVertices; v++)
        {
            if (currentSchedule.isScheduled(v))
            {
                // std::cout << "vertex: " << v << std::endl; // Debug
                
                // Get current vertex's neighbours
                uintT vDegree = GA.V[v].getOutDegree();
                uintT vMaxValue = vDegree + 1;
                bool scheduleNeighbors = false;
                activeEdges += vDegree;
                
                // Make bool array for possible color values and then set any color
                // already taken by neighbours to false 
                DenseBitset possibleValues(vMaxValue);
                possibleValues.setAll();
                cilk_for(uintT n = 0; n < vDegree; n++)
                {
                    intT i = GA.V[v].getOutNeighbor(n);
                    uint64_t neighVal = colorData[i].color;
                    possibleValues.set(neighVal, false);
                }

                // std::cout << std::endl; // Debug

                // Find minimum color by iterating through color array in increasing order
                uint64_t newColor = 0;
                uint64_t currentColor = colorData[v].color; 
                while (newColor < vMaxValue)
                {
                    // std::cout << "Color: " << newColor << " is " << possibleValues[newColor] << std::endl; // Debug 
                    
                    // If color is available and it is not the vertex's current value then try to assign
                    if (possibleValues.get(newColor))
                    {
                        if (currentColor != newColor)
                        {
                            // std::cout << "Does not equal: " << graph.getVertexValue(v) << " != " << newColor << std::endl; // Debug
                            colorData[v].color = newColor;
                            scheduleNeighbors = true;
                        }
                        
                        break;
                    }

                    newColor++;
                }

                // Schedule all neighbours if required
                if (scheduleNeighbors)
                {
                    for (uintT n = 0; n < vDegree; n++)
                    {
                        intT i = GA.V[v].getOutNeighbor(n);
                        currentSchedule.schedule(i, false);
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


    // assessGraph(GA, colorData);
}


