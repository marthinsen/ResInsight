/////////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2017 Statoil ASA
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

#include "RifHdf5SummaryReader.h"

#include "RiaLogging.h"
#include "RiaQDateTimeTools.h"
#include "RiaTimeTTools.h"
#include "RifHdf5Reader.h"

#include "H5Cpp.h"

#include <QDateTime>
#include <QString>

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RifHdf5SummaryReader::RifHdf5SummaryReader( const QString& fileName )
{
    try
    {
        m_hdfFile = new H5::H5File( fileName.toStdString().c_str(), H5F_ACC_RDONLY );
    }
    catch ( ... )
    {
        RiaLogging::error( "Failed to open HDF file " + fileName );

        delete m_hdfFile;
        m_hdfFile = nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RifHdf5SummaryReader::~RifHdf5SummaryReader()
{
    if ( m_hdfFile )
    {
        m_hdfFile->close();

        delete m_hdfFile;
        m_hdfFile = nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RifHdf5SummaryReader::isValid() const
{
    return ( m_hdfFile != nullptr );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::vector<std::string> RifHdf5SummaryReader::vectorNames()
{
    if ( m_hdfFile )
    {
        std::vector<std::string> names;

        auto groupNames = RifHdf5ReaderTools::getSubGroupNames( m_hdfFile, "summary_vectors" );
        names.reserve( groupNames.size() );
        for ( const auto& name : groupNames )
        {
            names.push_back( name );
        }

        return names;
    }

    return {};
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::vector<time_t> RifHdf5SummaryReader::timeSteps() const
{
    std::vector<time_t> times;

    // TODO: This function uses code taken from ESmry::dates(). There is conversion from time_t to
    // chrono::system_clock::time_points, and then back to time_t again. Consider using one representation

    double time_unit = 24 * 3600;

    using namespace std::chrono;
    using TP      = time_point<system_clock>;
    using DoubSec = duration<double, seconds::period>;

    auto timeDeltasInDays = values( "TIME" );

    std::vector<std::chrono::system_clock::time_point> timePoints;

    TP startDat = std::chrono::system_clock::from_time_t( startDate() );

    timePoints.reserve( timeDeltasInDays.size() );
    for ( const auto& t : timeDeltasInDays )
    {
        timePoints.push_back( startDat + duration_cast<TP::duration>( DoubSec( t * time_unit ) ) );
    }

    for ( const auto& d : timePoints )
    {
        auto timeAsTimeT = std::chrono::system_clock::to_time_t( d );
        times.push_back( timeAsTimeT );
    }

    return times;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::vector<double> RifHdf5SummaryReader::values( const std::string& vectorName, int smspecKeywordIndex ) const
{
    if ( m_hdfFile )
    {
        try
        {
            H5::Exception::dontPrint(); // Turn off auto-printing of failures to handle the errors appropriately

            std::string idText = std::to_string( smspecKeywordIndex );

            std::string groupPath = "summary_vectors/" + vectorName + "/" + idText;

            {
                H5::Group   generalGroup = m_hdfFile->openGroup( groupPath.c_str() );
                H5::DataSet dataset      = H5::DataSet( generalGroup.openDataSet( "values" ) );

                hsize_t       dims[2];
                H5::DataSpace dataspace = dataset.getSpace();
                dataspace.getSimpleExtentDims( dims, nullptr );

                std::vector<double> values;
                values.resize( dims[0] );
                dataset.read( values.data(), H5::PredType::NATIVE_DOUBLE );

                return values;
            }
        }
        catch ( ... )
        {
        }
    }

    return {};
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::vector<double> RifHdf5SummaryReader::values( const std::string& vectorName ) const
{
    if ( m_hdfFile )
    {
        try
        {
            H5::Exception::dontPrint(); // Turn off auto-printing of failures to handle the errors appropriately

            std::string groupPath = "summary_vectors/" + vectorName;

            auto groupNames = RifHdf5ReaderTools::getSubGroupNames( m_hdfFile, groupPath );
            if ( !groupNames.empty() )
            {
                groupPath = groupPath + "/" + groupNames[0];

                H5::Group   generalGroup = m_hdfFile->openGroup( groupPath.c_str() );
                H5::DataSet dataset      = H5::DataSet( generalGroup.openDataSet( "values" ) );

                hsize_t       dims[2];
                H5::DataSpace dataspace = dataset.getSpace();
                dataspace.getSimpleExtentDims( dims, nullptr );

                std::vector<double> values;
                values.resize( dims[0] );
                dataset.read( values.data(), H5::PredType::NATIVE_DOUBLE );

                return values;
            }
        }
        catch ( ... )
        {
        }
    }

    return {};
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
time_t RifHdf5SummaryReader::startDate() const
{
    if ( m_hdfFile )
    {
        try
        {
            H5::Exception::dontPrint(); // Turn off auto-printing of failures to handle the errors appropriately

            QString groupPath = QString( "general" );

            H5::Group   GridFunction_00002 = m_hdfFile->openGroup( groupPath.toStdString().c_str() );
            H5::DataSet dataset            = H5::DataSet( GridFunction_00002.openDataSet( "start_date" ) );

            hsize_t       dims[2];
            H5::DataSpace dataspace = dataset.getSpace();
            dataspace.getSimpleExtentDims( dims, nullptr );

            std::vector<int> values;
            values.resize( dims[0] );
            dataset.read( values.data(), H5::PredType::NATIVE_INT );

            int day    = values[0];
            int month  = values[1];
            int year   = values[2];
            int hour   = values[3];
            int minute = values[4];
            int second = values[5];

            QDate date( year, month, day );
            QTime time( hour, minute, second );

            QDateTime dt( date, time );

            QDateTime reportDateTime = RiaQDateTimeTools::createUtcDateTime( dt );

            time_t myTime = RiaTimeTTools::fromQDateTime( reportDateTime );
            return myTime;
        }
        catch ( ... )
        {
        }
    }

    RiaLogging::error( "Not able to read start_date from HDF5 " );

    return {};
}