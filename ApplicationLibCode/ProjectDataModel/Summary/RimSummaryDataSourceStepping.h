////////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2021-     Equinor ASA
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

#pragma once

#include <vector>

class RimSummaryCurve;
class RimEnsembleCurveSet;

class RimSummaryDataSourceStepping
{
public:
    enum class Axis
    {
        Y_AXIS,
        X_AXIS,
        UNION_X_Y_AXIS
    };

    enum class SourceSteppingDimension
    {
        SUMMARY_CASE,
        ENSEMBLE,
        WELL,
        GROUP,
        REGION,
        VECTOR,
        BLOCK,
        AQUIFER
    };

public:
    virtual std::vector<RimSummaryDataSourceStepping::Axis> availableAxes() const                                = 0;
    virtual std::vector<RimSummaryCurve*>     curvesForStepping( RimSummaryDataSourceStepping::Axis axis ) const = 0;
    virtual std::vector<RimSummaryCurve*>     allCurves( RimSummaryDataSourceStepping::Axis axis ) const         = 0;
    virtual std::vector<RimEnsembleCurveSet*> curveSets() const                                                  = 0;
};