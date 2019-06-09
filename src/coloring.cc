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

#include "bitsetscheduler.h"
#include "ligra.h"
#include "gettime.h"

#define TIME_PRECISION 3

struct Color
{
    uint64_t color;

    Color() : color(0) {}
    Color(uint64_t val) : color(val) {}

    inline Color operator=(const Color &rhs)
    {
        color = rhs.color;
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

Color* colorData;

// Go through every vertex and check that it's color does not conflict with neighbours
// while also checking that each vertex is minimally colored
template <class vertex>
void assessGraph(graph<vertex> &GA, uintT maxDegree) 
{
    uintT numVertices = GA.n;
    uintT conflict = 0;
    uintT notMinimal = 0;

    parallel_for(uintT v_i = 0; v_i < numVertices; v_i++)
    {
        Color vValue = colorData[v_i];  
        uintT vDegree = GA.V[v_i].getOutDegree();
        std::vector<bool> possibleColors(maxDegree + 1, true);
        Color minimalColor = 0;
        bool neighConflict = false;

        parallel_for(uintT n_i = 0; n_i < vDegree; n_i++)
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
void randomizeColors(graph<vertex> &GA)
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

// Naive coloring implementation
template <class vertex>
void Compute(graph<vertex> &GA, commandLine P)
{
    const size_t numVertices = GA.n;
    colorData = new Color[numVertices];
    const uintT maxDegree = getMaxDeg(GA);
    randomizeColors(GA);

    // Verbose variables
    bool verbose = true;
    uintT activeVertices;
    uintT activeEdges;
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
                // Get current vertex's neighbours
                const uintT vDegree = GA.V[v_i].getOutDegree();
                const Color vMaxColor = vDegree + 1;
                bool scheduleNeighbors = false;
                
                activeEdges += vDegree;

                // Make bool array for possible color values and then set any color
                // already taken by neighbours to false
                std::vector<bool> possibleColors(maxDegree + 1, true);
                std::vector<Color> neighColors(vDegree);
                
                parallel_for(uintT n_i = 0; n_i < vDegree; n_i++)
                {
                    uintT neigh = GA.V[v_i].getOutNeighbor(n_i);
                    Color neighVal = colorData[neigh];
                    possibleColors[neighVal.color] = false;
                    neighColors[n_i] = neighVal;          
                }

                // Find minimum color by iterating through color array in increasing order
                Color newColor(0);
                Color currentColor = colorData[v_i]; 
                while (newColor < vMaxColor)
                {                    
                    // If color is available and it is not the vertex's current value then try to assign
                    if (possibleColors[newColor.color])
                    {
                        if (currentColor != newColor)
                        {
                            colorData[v_i] = newColor;
                            scheduleNeighbors = true;
                        }
                        break;
                    }
                    newColor++;
                }

                // Schedule all neighbours if required
                if (scheduleNeighbors)
                {
                    parallel_for (uintT n_i = 0; n_i < vDegree; n_i++)
                    {
                        uintT i = GA.V[v_i].getOutNeighbor(n_i);
                        if (neighColors[n_i] >= newColor)
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
    }
    if (verbose)
    {
        cout << "\nTotal Time : " << setprecision(TIME_PRECISION) << fullTimer.stop() << "\n";
    }

    // Assess graph and cleanup
    assessGraph(GA, maxDegree);
    delete[] colorData;
}