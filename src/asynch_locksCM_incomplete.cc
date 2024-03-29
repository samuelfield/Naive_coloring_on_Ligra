// This code is part of the project "Ligra: A Lightweight Graph Processing
// Framework for Shared Memory", presented at Principles and Practice of
// Parallel Programming, 2013.
// Copyright (c) 2013 Julian Shun and Guy Blelloch
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "coloring_base_locks.h"


// Naive coloring implementation
template <class vertex>
void Compute(graph<vertex> &GA, commandLine P)
{
    timer fullTimer, iterTimer;
    fullTimer.start();

    // Check that graph is undirected (out degree == in degree for all vertices)
    ensureUndirected(GA);

    const size_t numVertices = GA.n;
    Color* colorData = new Color[numVertices];
    const uintT maxDegree = setDegrees(GA, colorData);
    for (uintT i = 0; i < numVertices; i++)
    {
        colorData[i].color = maxDegree;
    }

    // Verbose variables
    bool verbose = true;
    uintT activeVertices;
    uintT activeEdges;
    uintT changedVertices;

    // Make new scheduler and schedule all vertices
    BitsetScheduler currentSchedule(numVertices);
    currentSchedule.reset();
    currentSchedule.scheduleAll();

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
        changedVertices = 0;

        currentSchedule.newIteration();
        activeVertices = currentSchedule.numTasks();

        // Parallel loop where each vertex is assigned a color
        parallel_for(uintT v_i = 0; v_i < numVertices; v_i++)
        {
            if (currentSchedule.isScheduled(v_i))
            {
                // Get current vertex's neighbours
                const uintT vDegree = GA.V[v_i].getOutDegree();
                const uintT vMaxColor = vDegree + 1;
                bool scheduleNeighbors = false;
                uintT newColor = 0;
                uintT currentColor = colorData[v_i].color; 
                
                activeEdges += vDegree;

                // Make bool array for possible color values and then set any color
                // already taken by neighbours to false
                std::vector<bool> possibleColors(maxDegree + 1, true);

                // Get colors (need write lock on self and reader locks on all neighbours)
                while (!GetPossibleColors(GA, colorData, possibleColors, v_i)) {}
                
                // Find minimum color by iterating through color array in increasing order
                while (newColor <= vMaxColor)
                {                    
                    // If color is available and it is not the vertex's current value then try to assign
                    if (possibleColors[newColor])
                    {
                        if (currentColor != newColor)
                        {
                            colorData[v_i].color = newColor;
                            scheduleNeighbors = true;
                            changedVertices++;
                        }
                        break;
                    }
                    newColor++;
                }

                // Release locks
                releaseLocks(GA, colorData, v_i);

                // Schedule all neighbours if required
                if (scheduleNeighbors)
                {
                    for (uintT n_i = 0; n_i < vDegree; n_i++)
                    {
                        uintT neigh = GA.V[v_i].getOutNeighbor(n_i);
                        currentSchedule.schedule(neigh, false);
                    }
                }
            }
        }
        if (verbose)
        {
            std::cout << "\tActive Vs: " << activeVertices << std::endl;
            std::cout << "\tActive Es: " << activeEdges << std::endl;
            std::cout << "\tModified Vs: " << changedVertices << std::endl;
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