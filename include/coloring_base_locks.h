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

uintT vertexPriority;

struct Color
{
    uintT color;
    RWLock rwLock;
    uintT priority;
    uintT degree;

    Color() : color(0), degree(0)
    {
        priority = vertexPriority;
        vertexPriority++;
        rwLock.init();
    }
    Color(uint64_t val) : color(val), degree(0)
    {
        priority = vertexPriority;
        vertexPriority++;
        rwLock.init();
    }
    Color(const Color &rhs) : color(rhs.color), degree(0)
    {
        priority = rhs.priority;
    }

    inline Color operator=(const Color &rhs)
    {
        color = rhs.color;
        color = rhs.degree;
        priority = rhs.priority;
        return *this;
    }

    // prefix
    inline Color& operator++()
    {
        this->color++;
        return *this;
    }

    // postfix
    inline Color operator++(int)
    {
        Color tmp(*this);
        operator++();
        return tmp;
    }

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
    inline bool operator<(const Color &rhs)
    {
        if (color < rhs.color)
            return true;
        else
            return false;
    }
    inline bool operator<=(const Color &rhs)
    {
        if (color <= rhs.color)
            return true;
        else
            return false;
    }
    inline bool operator>(const Color &rhs)
    {
        if (color > rhs.color)
            return true;
        else
            return false;
    }
    inline bool operator>=(const Color &rhs)
    {
        if (color >= rhs.color)
            return true;
        else
            return false;
    }
};


template <class vertex>
void releaseLocks(graph<vertex> &GA, Color* &colorData, const uint v_i)
{
    const uintT vDegree = GA.V[v_i].getOutDegree();
    uintT neigh;

    colorData[v_i].rwLock.unlock();
    for(uintT n_i = 0; n_i < vDegree; n_i++)
    {
        neigh = GA.V[v_i].getOutNeighbor(n_i);
        colorData[neigh].rwLock.unlock();
    }
}

template <class vertex>
void releaseLocks_RC(graph<vertex> &GA, Color* &colorData, const uint v_i)
{
    colorData[v_i].rwLock.unlock();
}

template <class vertex>
bool GetPossibleColors( const graph<vertex> &GA,
                        Color* &colorData,
                        std::vector<bool> &possibleColors,
                        const uint v_i)
{
    const uintT vDegree = GA.V[v_i].getOutDegree();
    uintT neigh;
    int result;

    // Get write lock on self and reader locks on all neighbours
    colorData[v_i].rwLock.writeLock();
    for(uintT n_i = 0; n_i < vDegree; n_i++)
    {
        neigh = GA.V[v_i].getOutNeighbor(n_i);
        while ((result = colorData[neigh].rwLock.tryReadLock()) != 0)
        {
            if (result == EBUSY)
            {
                // Die if lower priority than neighbour
                // if (colorData[v_i].priority < colorData[neigh].priority)
                if (colorData[v_i].degree < colorData[neigh].degree || 
                    (colorData[v_i].degree == colorData[neigh].degree &&
                    colorData[v_i].priority < colorData[neigh].priority))
                {
                    // Release any locks that have been obtained
                    colorData[v_i].rwLock.unlock();

                    // Release locks from 1 before the conflict to beginning
                    for(uintT rev_i = n_i - 1; rev_i >= 0 && rev_i <= n_i; rev_i--)
                    {
                        // cout << v_i << ": " << rev_i << endl;
                        uintT rev_neigh = GA.V[v_i].getOutNeighbor(rev_i);
                        colorData[rev_neigh].rwLock.unlock();
                    }
                    return false;
                }
            }    
            else
            {
                cout << "Locking Error: " << result << endl;
                exit(0);
            }                  
        }

        uintT neighVal = colorData[neigh].color;
        possibleColors[neighVal] = false;  
    }

    return true;
}


template <class vertex>
bool GetPossibleColors_RC( const graph<vertex> &GA,
                        Color* &colorData,
                        std::vector<bool> &possibleColors,
                        const uint v_i)
{
    const uintT vDegree = GA.V[v_i].getOutDegree();
    uintT neigh;
    int result;

    // Get write lock on self and reader locks on all neighbours
    colorData[v_i].rwLock.writeLock();
    for(uintT n_i = 0; n_i < vDegree; n_i++)
    {
        neigh = GA.V[v_i].getOutNeighbor(n_i);
        while ((result = colorData[neigh].rwLock.tryReadLock()) != 0)
        {
            if (result == EBUSY)
            {
                // Die if lower priority than neighbour
                // if (colorData[v_i].priority < colorData[neigh].priority)
                if (colorData[v_i].degree < colorData[neigh].degree || 
                    (colorData[v_i].degree == colorData[neigh].degree &&
                    colorData[v_i].priority < colorData[neigh].priority))
                {
                    // Release any locks that have been obtained
                    colorData[v_i].rwLock.unlock();

                    // Release locks from 1 before the conflict to beginning
                    for(uintT rev_i = n_i - 1; rev_i >= 0 && rev_i <= n_i; rev_i--)
                    {
                        // cout << v_i << ": " << rev_i << endl;
                        uintT rev_neigh = GA.V[v_i].getOutNeighbor(rev_i);
                        colorData[rev_neigh].rwLock.unlock();
                    }
                    return false;
                }
            }    
            else
            {
                cout << "Locking Error: " << result << endl;
                exit(0);
            }                  
        }

        uintT neighVal = colorData[neigh].color;
        possibleColors[neighVal] = false;
        colorData[neigh].rwLock.unlock();
    }

    return true;
}


// Go through every vertex and check that it's color does not conflict with neighbours
// while also checking that each vertex is minimally colored
template <class vertex>
void assessGraph(graph<vertex> &GA, Color* &colorData, uintT maxDegree) 
{
    uintT numVertices = GA.n;
    uintT conflict = 0;
    uintT notMinimal = 0;

    parallel_for(uintT v_i = 0; v_i < numVertices; v_i++)
    {
        Color vValue = colorData[v_i];  
        uintT vDegree = GA.V[v_i].getOutDegree();
        std::vector<bool> possibleColors(maxDegree + 1, true);

        // Check for conflict and set possible colors
        bool neighConflict = false;
        for(uintT n_i = 0; n_i < vDegree; n_i++)
        {
            uintT neigh = GA.V[v_i].getOutNeighbor(n_i);
            Color neighVal = colorData[neigh];
            possibleColors[neighVal.color] = false;
            
            if (neighVal == vValue)
            {
                neighConflict = true;
            }
        }
        if (neighConflict)
            conflict++;

        // Check for minimality
        Color minimalColor = 0;
        while (!possibleColors[minimalColor.color] && (minimalColor < vDegree + 1))
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
    }
}


// Find the maximum degree amongst nodes of the graph
template <class vertex>
uintT setDegrees(const graph<vertex> &GA, Color* &colorData)
{
    uintT maxDegree = 0;
    for (uintT v_i=0; v_i < GA.n; v_i++)
    {
        colorData[v_i].degree = GA.V[v_i].getOutDegree();
        if (colorData[v_i].degree > maxDegree)
        {
            maxDegree = colorData[v_i].degree;
        }
    }
    return maxDegree;
}



//randomize vertex values
template <class vertex>
void randomizeColors(graph<vertex> &GA, Color* &colorData)
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