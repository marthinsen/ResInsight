/////////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2023     Equinor ASA
//
//  ResInsight is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  ResInsight is distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.
//
//  See the GNU General Public License at <http://www.gnu.org/licenses/gpl.html>
//  for more details.
//
/////////////////////////////////////////////////////////////////////////////////

#include "RigGriddedPart3d.h"

#include "cvfTextureImage.h"

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RigGriddedPart3d::RigGriddedPart3d( bool flipFrontBack )
    : m_flipFrontBack( flipFrontBack )
{
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RigGriddedPart3d::~RigGriddedPart3d()
{
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RigGriddedPart3d::reset()
{
    m_boundaryElements.clear();
    m_boundaryNodes.clear();
    m_borderSurfaceElements.clear();
    m_nodes.clear();
    m_elementIndices.clear();
    m_meshLines.clear();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
cvf::Vec3d RigGriddedPart3d::stepVector( cvf::Vec3d start, cvf::Vec3d stop, int nSteps )
{
    cvf::Vec3d vec = stop - start;
    return vec.getNormalized() * ( vec.length() / nSteps );
}

//--------------------------------------------------------------------------------------------------
///  Point index in input
///
///
///      3 ----------- 7                   *
///        |         |                     *
///        |         |                     *
///        |         |                     *
///      2 |---------| 6                   *
///        |         \                     *
///        |          \                    *
///        |           \                   *
///      1 -------------| 5                *
///        |            |                  *
///        |            |                  *
///        |            |                  *
///        |            |                  *
///      0 -------------- 4                *
///
/// Assumes 0->4, 1->5, 2->6 and 3->7 is parallel
///
///
//--------------------------------------------------------------------------------------------------
void RigGriddedPart3d::generateGeometry( std::vector<cvf::Vec3d> inputPoints,
                                         int                     nHorzCells,
                                         int                     nVertCellsLower,
                                         int                     nVertCellsMiddle,
                                         int                     nVertCellsUpper,
                                         double                  thickness )
{
    reset();

    cvf::Vec3d step0to1 = stepVector( inputPoints[0], inputPoints[1], nVertCellsLower );
    cvf::Vec3d step1to2 = stepVector( inputPoints[1], inputPoints[2], nVertCellsMiddle );
    cvf::Vec3d step2to3 = stepVector( inputPoints[2], inputPoints[3], nVertCellsUpper );

    cvf::Vec3d step4to5 = stepVector( inputPoints[4], inputPoints[5], nVertCellsLower );
    cvf::Vec3d step5to6 = stepVector( inputPoints[5], inputPoints[6], nVertCellsMiddle );
    cvf::Vec3d step6to7 = stepVector( inputPoints[6], inputPoints[7], nVertCellsUpper );

    cvf::Vec3d step0to4 = stepVector( inputPoints[0], inputPoints[4], nHorzCells );

    cvf::Vec3d tVec = step0to4 ^ step0to1;
    tVec.normalize();
    tVec *= thickness;

    const std::vector<double> m_thicknessFactors = { -1.0, 0.0, 1.0 };
    const int                 nThicknessCells    = 2;
    const int                 nVertCells         = nVertCellsLower + nVertCellsMiddle + nVertCellsUpper;

    const std::vector<int>        vertLines  = { nVertCellsLower, nVertCellsMiddle, nVertCellsUpper + 1 };
    const std::vector<cvf::Vec3d> firstSteps = { step0to1, step1to2, step2to3 };
    const std::vector<cvf::Vec3d> lastSteps  = { step4to5, step5to6, step6to7 };

    const Boundary boundaryBack  = m_flipFrontBack ? Boundary::Front : Boundary::Back;
    const Boundary boundaryFront = m_flipFrontBack ? Boundary::Back : Boundary::Front;

    // ** generate nodes

    m_boundaryNodes[boundaryFront]     = {};
    m_boundaryNodes[boundaryBack]      = {};
    m_boundaryNodes[Boundary::Bottom]  = {};
    m_boundaryNodes[Boundary::FarSide] = {};

    m_nodes.reserve( (size_t)( ( nVertCells + 1 ) * ( nHorzCells + 1 ) ) );

    cvf::Vec3d pFrom = inputPoints[0];
    cvf::Vec3d pTo   = inputPoints[4];

    unsigned int layer     = 0;
    unsigned int nodeIndex = 0;

    for ( int i = 0; i < (int)vertLines.size(); i++ )
    {
        for ( int v = 0; v < vertLines[i]; v++, layer++ )
        {
            cvf::Vec3d stepHorz = stepVector( pFrom, pTo, nHorzCells );
            cvf::Vec3d p        = pFrom;
            for ( int h = 0; h <= nHorzCells; h++ )
            {
                for ( int t = 0; t <= nThicknessCells; t++, nodeIndex++ )
                {
                    m_nodes.push_back( p + m_thicknessFactors[t] * tVec );
                    if ( layer == 0 )
                    {
                        m_boundaryNodes[Boundary::Bottom].push_back( nodeIndex );
                    }
                    if ( h == 0 )
                    {
                        m_boundaryNodes[Boundary::FarSide].push_back( nodeIndex );
                    }
                    if ( t == 0 )
                    {
                        m_boundaryNodes[boundaryFront].push_back( nodeIndex );
                    }
                    else if ( t == nThicknessCells )
                    {
                        m_boundaryNodes[boundaryBack].push_back( nodeIndex );
                    }
                }

                p += stepHorz;
            }
            pFrom += firstSteps[i];
            pTo += lastSteps[i];
        }
    }

    // ** generate elements of type hex8

    m_elementIndices.resize( (size_t)( nVertCells * nHorzCells * nThicknessCells ) );

    m_borderSurfaceElements[BorderSurface::UpperSurface] = {};
    m_borderSurfaceElements[BorderSurface::FaultSurface] = {};
    m_borderSurfaceElements[BorderSurface::LowerSurface] = {};

    m_boundaryElements[boundaryFront]     = {};
    m_boundaryElements[boundaryBack]      = {};
    m_boundaryElements[Boundary::Bottom]  = {};
    m_boundaryElements[Boundary::FarSide] = {};

    int layerIndexOffset = 0;
    int elementIdx       = 0;
    layer                = 0;

    BorderSurface currentSurfaceRegion = BorderSurface::LowerSurface;

    const int nextLayerIdxOff = ( nHorzCells + 1 ) * ( nThicknessCells + 1 );
    const int nThicknessOff   = nThicknessCells + 1;

    for ( int v = 0; v < nVertCells; v++, layer++ )
    {
        if ( v >= nVertCellsLower ) currentSurfaceRegion = BorderSurface::FaultSurface;
        if ( v >= nVertCellsLower + nVertCellsMiddle ) currentSurfaceRegion = BorderSurface::UpperSurface;

        int i = layerIndexOffset;

        for ( int h = 0; h < nHorzCells; h++ )
        {
            for ( int t = 0; t < nThicknessCells; t++, elementIdx++ )
            {
                m_elementIndices[elementIdx].push_back( t + i );
                m_elementIndices[elementIdx].push_back( t + i + nThicknessOff );
                m_elementIndices[elementIdx].push_back( t + i + nThicknessOff + 1 );
                m_elementIndices[elementIdx].push_back( t + i + 1 );

                m_elementIndices[elementIdx].push_back( t + nextLayerIdxOff + i );
                m_elementIndices[elementIdx].push_back( t + nextLayerIdxOff + i + nThicknessOff );
                m_elementIndices[elementIdx].push_back( t + nextLayerIdxOff + i + nThicknessOff + 1 );
                m_elementIndices[elementIdx].push_back( t + nextLayerIdxOff + i + 1 );

                if ( layer == 0 )
                {
                    m_boundaryElements[Boundary::Bottom].push_back( elementIdx );
                }
                if ( h == 0 )
                {
                    m_boundaryElements[Boundary::FarSide].push_back( elementIdx );
                }
                if ( t == 0 )
                {
                    m_boundaryElements[boundaryFront].push_back( elementIdx );
                }
                else if ( t == ( nThicknessCells - 1 ) )
                {
                    m_boundaryElements[boundaryBack].push_back( elementIdx );
                }
            }
            i += nThicknessOff;
        }

        // add elements to border surface in current region
        m_borderSurfaceElements[currentSurfaceRegion].push_back( elementIdx - 2 );
        m_borderSurfaceElements[currentSurfaceRegion].push_back( elementIdx - 1 );

        layerIndexOffset += nextLayerIdxOff;
    }

    // generate meshlines for 2d viz

    generateMeshlines( { inputPoints[0], inputPoints[1], inputPoints[5], inputPoints[4] }, nHorzCells, nVertCellsLower );
    generateMeshlines( { inputPoints[1], inputPoints[2], inputPoints[6], inputPoints[5] }, nHorzCells, nVertCellsMiddle );
    generateMeshlines( { inputPoints[2], inputPoints[3], inputPoints[7], inputPoints[6] }, nHorzCells, nVertCellsUpper );
}

//--------------------------------------------------------------------------------------------------
///  Point index in input
///
///     1 ____________ 2
///      |           /
///      |          /
///      |         /
///      |        /
///      |_______/
///      0         3
///
/// Assumes 0->3 and 1->2 is parallel
//--------------------------------------------------------------------------------------------------
void RigGriddedPart3d::generateMeshlines( std::vector<cvf::Vec3d> cornerPoints, int numHorzCells, int numVertCells )
{
    cvf::Vec3d step0to1 = stepVector( cornerPoints[0], cornerPoints[1], numVertCells );
    cvf::Vec3d step0to3 = stepVector( cornerPoints[0], cornerPoints[3], numHorzCells );
    cvf::Vec3d step1to2 = stepVector( cornerPoints[1], cornerPoints[2], numHorzCells );
    cvf::Vec3d step3to2 = stepVector( cornerPoints[3], cornerPoints[2], numVertCells );

    // horizontal lines

    cvf::Vec3d startP = cornerPoints[0];
    cvf::Vec3d endP   = cornerPoints[3];

    for ( int v = 0; v <= numVertCells; v++ )
    {
        m_meshLines.push_back( { startP, endP } );
        startP += step0to1;
        endP += step3to2;
    }

    // vertical lines

    startP = cornerPoints[0];
    endP   = cornerPoints[1];

    for ( int h = 0; h <= numHorzCells; h++ )
    {
        m_meshLines.push_back( { startP, endP } );
        startP += step0to3;
        endP += step1to2;
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
const std::vector<cvf::Vec3d>& RigGriddedPart3d::nodes() const
{
    return m_nodes;
}

//--------------------------------------------------------------------------------------------------
/// Output elements will be of type HEX8
///
///     7---------6
///    /|        /|
///   / |       / |
///  4---------5  |     z
///  |  3------|--2       |   y
///  | /       | /        | /
///  |/        |/         |/
///  0---------1           ----- x
///
//--------------------------------------------------------------------------------------------------
const std::vector<std::vector<unsigned int>>& RigGriddedPart3d::elementIndices() const
{
    return m_elementIndices;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
const std::map<RigGriddedPart3d::BorderSurface, std::vector<unsigned int>>& RigGriddedPart3d::borderSurfaceElements() const
{
    return m_borderSurfaceElements;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
const std::vector<std::vector<cvf::Vec3d>>& RigGriddedPart3d::meshLines() const
{
    return m_meshLines;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
const std::map<RigGriddedPart3d::Boundary, std::vector<unsigned int>>& RigGriddedPart3d::boundaryElements() const
{
    return m_boundaryElements;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
const std::map<RigGriddedPart3d::Boundary, std::vector<unsigned int>>& RigGriddedPart3d::boundaryNodes() const
{
    return m_boundaryNodes;
}