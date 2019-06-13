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

#include "coloring_base.h"


// Naive coloring implementation
template <class vertex>
void Compute(graph<vertex> &GA, commandLine P)
{
    // Check that graph is undirected (out degree == in degree for all vertices)
    ensureUndirected(GA);
    
    const size_t numVertices = GA.n;
    colorData = new Color[numVertices];
    const uintT maxDegree = getMaxDeg(GA);

    // Verbose variables
    bool verbose = true;
    uintT activeVertices;
    uintT activeEdges;
    timer fullTimer, iterTimer;

    // Make new scheduler and schedule all vertices
    BitsetScheduler currentSchedule(numVertices);
    currentSchedule.reset();
    currentSchedule.scheduleAll();

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
        parallel_for(uintT v_i = 0; v_i < numVertices; v_i++)
        {
            if (currentSchedule.isScheduled(v_i))
            {
                // Get current vertex's neighbours
                const uintT vDegree = GA.V[v_i].getOutDegree();
                const Color vMaxColor = vDegree + 1;
                bool scheduleNeighbors = false;
                
                activeEdges += vDegree;

                // Make bool array for possible color values and then set any color
                // already taken by neighbours to false
                std::vector<bool> possibleColors(maxDegree + 1, true);
                
                for(uintT n_i = 0; n_i < vDegree; n_i++)
                {
                    uintT neigh = GA.V[v_i].getOutNeighbor(n_i);
                    Color neighVal = colorData[neigh]; // Probably race condition here without locks   
                    if (possibleColors[neighVal.color]) // Add check to avoid false sharing
                        possibleColors[neighVal.color] = false;      
                }

                // Find minimum color by iterating through color array in increasing order
                Color newColor(0);
                Color currentColor = colorData[v_i]; 
                while (newColor <= vMaxColor)
                {                    
                    // If color is available and it is not the vertex's current value then try to assign
                    if (possibleColors[newColor.color])
                    {
                        if (currentColor != newColor)
                        {
                            scheduleNeighbors = true;
                        }
                        colorData[v_i] = newColor;
                        break;
                    }
                    newColor++;
                }

                // Schedule all neighbours if required
                if (scheduleNeighbors)
                {
                    parallel_for (uintT n_i = 0; n_i < vDegree; n_i++)
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
            std::cout << "\tTime: " << setprecision(TIME_PRECISION) << iterTimer.getTime() - lastStopTime << std::endl;
            lastStopTime = iterTimer.getTime();
        }
    }
    if (verbose)
    {
        cout << "\nTotal Time : " << setprecision(TIME_PRECISION) << fullTimer.stop() << "\n";
    }

    // Assess graph and cleanup
    assessGraph(GA, maxDegree);
    delete[] colorData;
}