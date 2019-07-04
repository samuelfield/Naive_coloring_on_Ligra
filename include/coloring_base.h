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

#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <random>
#include <ctime>

#include "bitsetscheduler.h"
#include "ligra.h"
#include "gettime.h"

#define TIME_PRECISION 3


// Go through every vertex and check that it's color does not conflict with neighbours
// while also checking that each vertex is minimally colored
template <class vertex>
void assessGraph(const graph<vertex> &GA, const std::vector<uintT> &colorData, const uintT maxDegree) 
{
    const uintT numVertices = GA.n;
    uintT conflict = 0;
    uintT notMinimal = 0;
    uintT maxColor = 0;

    parallel_for (uintT v_i = 0; v_i < numVertices; v_i++)
    {
        uintT vValue = colorData[v_i];  
        uintT vDegree = GA.V[v_i].getOutDegree();
        std::vector<bool> possibleColors(maxDegree + 1, true);
        if (vValue > maxColor)
            maxColor = vValue;

        // Check for conflict and set possible colors
        bool neighConflict = false;
        for(uintT n_i = 0; n_i < vDegree; n_i++)
        {
            uintT neigh = GA.V[v_i].getOutNeighbor(n_i);
            uintT neighVal = colorData[neigh];
            possibleColors[neighVal] = false;
            
            if (neighVal == vValue)
            {
                neighConflict = true;
            }
        }
        if (neighConflict)
            conflict++;

        // Check for minimality
        uintT minimalColor = 0;
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
    if (notMinimal != 0)
    {
        std::cout << "Failure: minimality condition broken for " << notMinimal << " vertices" << std::endl;
    }
    if (conflict == 0 && notMinimal == 0)
    {
        std::cout << "Successful Coloring!" << std::endl;
        std::cout << "Max Color: " << maxColor <<  "\tMax Degree: " << maxDegree << std::endl;
    }
}


// Find the maximum degree amongst nodes of the graph
template <class vertex>
uintT getMaxDeg(const graph<vertex> &GA)
{
    uintT maxDegree = 0;
    // TODO: look into parallelizing this loop with CAS
    for (uintT v_i=0; v_i < GA.n; v_i++)
    {
        if (GA.V[v_i].getOutDegree() > maxDegree)
        {
            maxDegree = GA.V[v_i].getOutDegree();
        }
    }
    return maxDegree;
}



//randomize vertex values
template <class vertex>
void randomizeColors(graph<vertex> &GA, std::vector<uintT> &colorData)
{
    const size_t numVertices = GA.n;
    std::random_device rd;
    std::mt19937 mt(rd());

    for (uintT v_i = 0; v_i < numVertices; v_i++)
    {
        const uintT vDegree = GA.V[v_i].getOutDegree();
        std::uniform_int_distribution<uintT> dist(0, vDegree);
        colorData[v_i] = dist(mt);
    }
}




//Check graph is undirected
template <class vertex>
void ensureUndirected(graph<vertex> &GA)
{
    for(uintT v_i = 0; v_i < GA.n; v_i++)
    { 
        const uintT outVDegree = GA.V[v_i].getOutDegree();
        const uintT inVDegree = GA.V[v_i].getInDegree();

        if (outVDegree != inVDegree)
        {
            cout << "Graph is not undirected. Exiting..." << endl;
            exit(2);
        }
    }
}

template <class vertex>
uintT makeColorPartition(graph<vertex> &GA,
                        std::vector<std::vector<uintT>> &partition,
                        std::vector<uintT> &colorData,
                        uintT maxDegree)
{
    uintT changedVertices = 0;
    const size_t numVertices = GA.n;
    for(uintT v_i = 0; v_i < numVertices; v_i++)
    {
        // Get current vertex's neighbours
        const uintT vDegree = GA.V[v_i].getOutDegree();        

        // Make bool array for possible color values and then set any color
        // already taken by neighbours to false
        std::vector<bool> possibleColors(maxDegree + 1, true);
        
        for (uintT n_i = 0; n_i < vDegree; n_i++)
        {
            uintT neigh = GA.V[v_i].getOutNeighbor(n_i);
            uintT neighVal = colorData[neigh];
            possibleColors[neighVal] = false;  
        }

        // Find minimum color by iterating through color array in increasing order
        uintT newColor = 0;
        while (newColor <= vDegree)
        {                    
            // If color is available and it is not the vertex's current value then try to assign
            if (possibleColors[newColor])
            {
                colorData[v_i] = newColor;
                partition[newColor].push_back(v_i);
                changedVertices++;
                break;
            }
            newColor++;
        }
    }

    return changedVertices;
}