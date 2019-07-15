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
    std::vector<uintT> colorData(numVertices, maxDegree);
    // randomizeColors(GA, colorData);
    std::vector<uintT> potentialColor(numVertices, maxDegree);

    // Verbose variables
    bool verbose = true;
    uintT activeVertices;
    uintT activeEdges;
    uintT changedVertices;

    // Make new scheduler and schedule all vertices
    BitsetScheduler currentSchedule(numVertices);
    currentSchedule.reset();
    currentSchedule.scheduleAll(false);

    double lastStopTime = iterTimer.getTime();

    // Loop over vertices until nothing is scheduled
    uint64_t iter = 0;
    while (currentSchedule.anyScheduledTasks())
    {
        if (verbose)
        {
            iter++;
            std::cout << std::endl;
            std::cout << "Iteration: " << iter << std::endl;
        }
        activeVertices = 0;
        activeEdges = 0;
        changedVertices = 0;

        currentSchedule.newIteration();
        activeVertices = currentSchedule.numTasks();

        // Parallel loop where each vertex is assigned a color
        parallel_for (uintT currentNode = 0; currentNode < numVertices; currentNode++)
        {
            if (currentSchedule.isScheduled(currentNode))
            {
                // Get current vertex's neighbours
                const uintT vDegree = GA.V[currentNode].getOutDegree();
                
                activeEdges += vDegree;

                // Make bool array for possible color values and then set any color
                // already taken by neighbours to false
                std::vector<bool> possibleColors(maxDegree + 1, true);
                
                for(uintT n_i = 0; n_i < vDegree; n_i++)
                {
                    uintT neighbourNode = GA.V[currentNode].getOutNeighbor(n_i);
                    uintT neighVal = colorData[neighbourNode];  
                    possibleColors[neighVal] = false;
                }

                // Find minimum color by iterating through color array in increasing order
                uintT newColor = 0;
                uintT oldColor = colorData[currentNode]; 
                while (newColor <= vDegree)
                {                    
                    // If color is available and it is not the vertex's current value then try to assign
                    if (possibleColors[newColor])
                    {
                        if (oldColor != newColor)
                        {
                            potentialColor[currentNode] = newColor;
                            changedVertices++;
                        }
                        break;
                    }
                    newColor++;
                }

                // Verify that color change is non-conflicting
                bool colorChange = true;
                for (uintT n_i = 0; n_i < vDegree; n_i++)
                {
                    uintT neighbourNode = GA.V[currentNode].getOutNeighbor(n_i);
                    if (CAS(&potentialColor[currentNode], potentialColor[neighbourNode], oldColor)) // race here?
                    {
                        currentSchedule.schedule(currentNode, false);
                        colorChange = false;
                        break;
                    }
                }

                if (colorChange)
                {
                    colorData[currentNode] = potentialColor[currentNode];
                    for (uintT n_i = 0; n_i < vDegree; n_i++)
                    {
                        uintT neighbourNode = GA.V[currentNode].getOutNeighbor(n_i);
                        if (oldColor < colorData[neighbourNode])
                            currentSchedule.schedule(neighbourNode, false);
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