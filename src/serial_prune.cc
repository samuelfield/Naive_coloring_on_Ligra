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
    // randomizeColors(GA, colorData);
    std::vector<uintT> minimalColor(numVertices, 0);
    std::vector<uintT> colorData(numVertices); 
    for (uintT v_i = 0; v_i < numVertices; v_i++)
    {
        colorData[v_i] = GA.V[v_i].getOutDegree();
    }

    // Make neighbour lists
    std::vector<std::vector<listNode>> neighbours(numVertices);
    for (uintT v_i = 0; v_i < numVertices; v_i++)
    {
        uintT vDegree = GA.V[v_i].getOutDegree();

        neighbours[v_i].resize(vDegree + 2);

        // Set head and tail node
        neighbours[v_i][0].vertexID = -1;
        neighbours[v_i][0].nextNode = &neighbours[v_i][1];
        neighbours[v_i][0].prevNode = nullptr;

        neighbours[v_i][vDegree + 1].vertexID = -1;
        neighbours[v_i][vDegree + 1].nextNode = nullptr;
        neighbours[v_i][vDegree + 1].prevNode = &neighbours[v_i][vDegree];;


        for (uintT n_i = 0; n_i < vDegree; n_i++)
        {
            neighbours[v_i][n_i+1].vertexID = GA.V[v_i].getOutNeighbor(n_i);
            neighbours[v_i][n_i+1].nextNode = &neighbours[v_i][n_i+2];
            neighbours[v_i][n_i+1].prevNode = &neighbours[v_i][n_i];
        }
    }

    // Make map vertexID -> neighbours index for removing neighbours
    std::vector<std::unordered_map<uintT,uintT>> reverseNeighbours(numVertices);
    for (uintT v_i = 0; v_i < numVertices; v_i++)
    {
        uintT vDegree = GA.V[v_i].getOutDegree();
        for (uintT n_i = 0; n_i < vDegree; n_i++)
        {
            reverseNeighbours[v_i][GA.V[v_i].getOutNeighbor(n_i)] = n_i + 1;
        }
    }

    // Make bool array for possible color values and then set any color
    // already taken by neighbours to false
    std::vector<std::vector<uintT>> possibleColors(numVertices);
    for (uintT v_i = 0; v_i < numVertices; v_i++)
    {
        uintT vDegree = GA.V[v_i].getOutDegree();
        possibleColors[v_i].resize(vDegree + 1, 0);

        for (uintT n_i = 0; n_i < vDegree; n_i++)
        {
            uintT neigh = GA.V[v_i].getOutNeighbor(n_i);
            uintT neighDegree = GA.V[neigh].getOutDegree();
            if (vDegree >= neighDegree)
                possibleColors[v_i][neighDegree]++;
        }
    }

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
        changedVertices= 0;

        currentSchedule.newIteration();
        activeVertices = currentSchedule.numTasks();

        // Loop where each vertex is assigned a color
        for (uintT v_i = 0; v_i < numVertices; v_i++)
        {
            if (currentSchedule.isScheduled(v_i))
            {
                // Get current vertex's neighbours
                const uintT vDegree = GA.V[v_i].getOutDegree();
                bool scheduleNeighbors = false;
                bool removeFromNeigh = false;

                listNode* head = &neighbours[v_i][0];
                listNode* tail = &neighbours[v_i][vDegree+1];
                
                activeEdges += vDegree;

                // Find minimum color by iterating through color array in increasing order
                uintT newColor = minimalColor[v_i];
                uintT oldColor = colorData[v_i]; 
                while (newColor < oldColor)
                {                    
                    // If color is available and it is not the vertex's current value then try to assign
                    if (possibleColors[v_i][newColor] == 0)
                    {
                        colorData[v_i] = newColor;
                        if (newColor == minimalColor[v_i])
                            removeFromNeigh = true;
                        scheduleNeighbors = true;
                        changedVertices++;

                        break;
                    }
                    newColor++;
                }

                // Schedule all neighbours if required
                if (scheduleNeighbors)
                {
                    listNode* neighNode = head->nextNode;
                    while( neighNode != tail)
                    {
                        uintT neigh = neighNode->vertexID;
                        if (oldColor < colorData[neigh])
                            currentSchedule.schedule(neigh, false);

                        if (removeFromNeigh)
                        {
                            uintT indexToRemove = reverseNeighbours[neigh][v_i];
                            neighbours[neigh][indexToRemove].prevNode->nextNode = neighbours[neigh][indexToRemove].nextNode;
                            neighbours[neigh][indexToRemove].nextNode->prevNode = neighbours[neigh][indexToRemove].prevNode;

                            if (minimalColor[neigh] == newColor) 
                                minimalColor[neigh] = newColor + 1;
                        }

                        uintT neighDeg = GA.V[neigh].getOutDegree();
                        if (neighDeg >= newColor)
                            possibleColors[neigh][newColor]++;
                        if (neighDeg >= oldColor)
                            possibleColors[neigh][oldColor]--;
                        neighNode = neighNode->nextNode;
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