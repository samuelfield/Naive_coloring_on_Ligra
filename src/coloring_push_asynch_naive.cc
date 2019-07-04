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
    
    // Check that graph is undirected (out degree == in degree for all vertices)
    ensureUndirected(GA);
    
    const size_t numVertices = GA.n;
    const uintT maxDegree = getMaxDeg(GA);

    const uintT initialColor = 200;
    std::vector<uintT> currentColor(numVertices, initialColor);
    std::vector<uintT> potentialColor(numVertices, 0);
    std::vector<std::vector<uintT>> neighborColors(numVertices, std::vector<uintT>(initialColor));

    // Initialize neighbourColors so that each vertex has [initialColor] = # of neighbours
    for (uintT v_i = 0; v_i < numVertices; v_i++)
    {
        neighborColors[v_i][initialColor] =  GA.V[v_i].getOutDegree();
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
        parallel_for (uintT v_i = 0; v_i < numVertices; v_i++)
        {
            if (currentSchedule.isScheduled(v_i))
            {
                // Get current vertex's neighbours
                const uintT vDegree = GA.V[v_i].getOutDegree();
                activeEdges += vDegree;
                
                // Find minimum color by checking potential color and taking it if it is less
                if (potentialColor[v_i] < currentColor[v_i])
                {
                    uintT oldColor = currentColor[v_i];
                    uintT newColor = potentialColor[v_i];
                    currentColor[v_i] = potentialColor[v_i];
                    changedVertices++;
   
                    // Update with neighbours
                    for (uintT n_i = 0; n_i < vDegree; n_i++)
                    {
                        uintT neigh = GA.V[v_i].getOutNeighbor(n_i);
                        --neighborColors[neigh][oldColor];
                        ++neighborColors[neigh][newColor];

                        if (neighborColors[neigh][oldColor] == 0)
                        {
                            currentSchedule.schedule(neigh, false);
                        }

                        // If change to current node opened up better color for neighbour, neighbour takes it
                        if (neighborColors[neigh][oldColor] == 0 && oldColor < potentialColor[neigh])
                        {
                            potentialColor[neigh] = oldColor;
                        }
                        // If change to current node made potential color worse for neighbour, neighbour finds new potential.
                        else if (newColor == potentialColor[neigh])
                        {   
                            uintT neighPotentialColor = newColor;
                            while (neighborColors[neigh][neighPotentialColor] != 0)
                            {
                                neighPotentialColor++;
                            }
                            potentialColor[neigh] = neighPotentialColor;
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
    assessGraph(GA, currentColor, maxDegree);
}