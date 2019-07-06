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
    timer fullTimer, iterTimer;
    fullTimer.start();
    // Verbose variables
    bool verbose = true;
    
    // Check that graph is undirected (out degree == in degree for all vertices)
    ensureUndirected(GA);
    
    const size_t numVertices = GA.n;
    const uintT maxDegree = getMaxDeg(GA);

    const uintT maxColor = 500;
    std::vector<uintT> colorData(numVertices, maxColor);
    // randomizeColors(GA, colorData);
    std::vector<std::vector<uintT>> neighborColors(numVertices, std::vector<uintT>(maxColor));
    std::vector<std::mutex> colorLock(numVertices);
    
    // Initialize neighbourColors so that each vertex has [maxColor] = # of neighbours
    for (uintT v_i = 0; v_i < numVertices; v_i++)
    {
        neighborColors[v_i][maxColor] =  GA.V[v_i].getOutDegree();
    }

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
        uintT activeVertices = 0;
        uintT activeEdges = 0;
        uintT changedVertices = 0;

        currentSchedule.newIteration();
        activeVertices = currentSchedule.numTasks();

        // Parallel loop where each vertex is assigned a color
        parallel_for (uintT v_i = 0; v_i < numVertices; v_i++)
        {
            if (currentSchedule.isScheduled(v_i))
            {
                // Get current vertex's neighbours
                const uintT vDegree = GA.V[v_i].getOutDegree();
                const uintT vMaxColor = vDegree + 1;
                bool scheduleNeighbors = false;
                
                activeEdges += vDegree;

                // Find minimum color by iterating through color array in increasing order
                uintT newColor = 0;
                uintT oldColor = colorData[v_i];
                
                colorLock[v_i].lock();
                while (newColor <= vMaxColor)
                {                    
                    // If color is available and it is not the vertex's current value then try to assign
                    if (neighborColors[v_i][newColor] == 0)
                    {
                        if (oldColor != newColor)
                        {
                            colorData[v_i] = newColor;
                            scheduleNeighbors = true;
                            changedVertices++;
                        }
                        break;
                    }
                    newColor++;
                }
                colorLock[v_i].unlock();

                // Schedule all neighbours if required
                if (scheduleNeighbors)
                {
                    for (uintT n_i = 0; n_i < vDegree; n_i++)
                    {
                        uintT neigh = GA.V[v_i].getOutNeighbor(n_i);

                        colorLock[neigh].lock();
                        neighborColors[neigh][newColor]++;
                        neighborColors[neigh][oldColor]--;
                        colorLock[neigh].unlock();

                        if ((neighborColors[neigh][oldColor] == 0 && oldColor < colorData[neigh])
                             || colorData[v_i] == colorData[neigh])
                        {
                            currentSchedule.schedule(neigh, false);
                        }
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
}