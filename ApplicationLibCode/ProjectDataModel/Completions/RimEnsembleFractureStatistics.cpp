/////////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2021 -     Equinor ASA
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

#include "RimEnsembleFractureStatistics.h"

#include "RiaDefines.h"
#include "RiaLogging.h"
#include "RiaPreferences.h"

#include "RigFractureGrid.h"
#include "RigSlice2D.h"
#include "RigStatisticsMath.h"
#include "RigStimPlanFractureDefinition.h"

#include "RifCsvDataTableFormatter.h"
#include "RifEnsembleFractureStatisticsExporter.h"
#include "RifStimPlanXmlReader.h"

#include "FractureCommands/RicNewStimPlanFractureTemplateFeature.h"

#include "cafAppEnum.h"
#include "cafPdmUiTextEditor.h"
#include "cafPdmUiToolButtonEditor.h"

#include <cmath>

#include <QDir>
#include <QFile>

namespace caf
{
template <>
void caf::AppEnum<RimEnsembleFractureStatistics::StatisticsType>::setUp()
{
    addItem( RimEnsembleFractureStatistics::StatisticsType::MEAN, "MEAN", "Mean" );
    addItem( RimEnsembleFractureStatistics::StatisticsType::MIN, "MIN", "Minimum" );
    addItem( RimEnsembleFractureStatistics::StatisticsType::MAX, "MAX", "Maximum" );
    addItem( RimEnsembleFractureStatistics::StatisticsType::P10, "P10", "P10" );
    addItem( RimEnsembleFractureStatistics::StatisticsType::P50, "P50", "P50" );
    addItem( RimEnsembleFractureStatistics::StatisticsType::P90, "P90", "P90" );
    addItem( RimEnsembleFractureStatistics::StatisticsType::OCCURRENCE, "OCCURRENCE", "Occurrence" );
    setDefault( RimEnsembleFractureStatistics::StatisticsType::MEAN );
}

} // namespace caf

CAF_PDM_SOURCE_INIT( RimEnsembleFractureStatistics, "EnsembleFractureStatistics" );

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimEnsembleFractureStatistics::RimEnsembleFractureStatistics()
{
    CAF_PDM_InitObject( "Ensemble Fracture Statistics", ":/FractureTemplate16x16.png", "", "" );

    CAF_PDM_InitFieldNoDefault( &m_filePaths, "FilePaths", "", "", "", "" );

    CAF_PDM_InitFieldNoDefault( &m_filePathsTable, "FilePathsTable", "File Paths Table", "", "", "" );
    m_filePathsTable.uiCapability()->setUiEditorTypeName( caf::PdmUiTextEditor::uiEditorTypeName() );
    m_filePathsTable.uiCapability()->setUiLabelPosition( caf::PdmUiItemInfo::HIDDEN );
    m_filePathsTable.uiCapability()->setUiReadOnly( true );
    m_filePathsTable.xmlCapability()->disableIO();

    CAF_PDM_InitFieldNoDefault( &m_computeStatistics, "ComputeStatistics", "Compute", "", "", "" );
    m_computeStatistics.uiCapability()->setUiEditorTypeName( caf::PdmUiToolButtonEditor::uiEditorTypeName() );
    m_computeStatistics.uiCapability()->setUiLabelPosition( caf::PdmUiItemInfo::HIDDEN );

    CAF_PDM_InitField( &m_numSamplesX, "NumberOfSamplesX", 100, "X", "", "", "" );
    CAF_PDM_InitField( &m_numSamplesY, "NumberOfSamplesY", 200, "Y", "", "", "" );

    setDeletable( true );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimEnsembleFractureStatistics::~RimEnsembleFractureStatistics()
{
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEnsembleFractureStatistics::addFilePath( const QString& filePath )
{
    m_filePaths.v().push_back( filePath );
    m_filePathsTable = generateFilePathsTable();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QString RimEnsembleFractureStatistics::generateFilePathsTable()
{
    QString body;
    for ( auto prop : m_filePaths.v() )
    {
        body.append( prop.path() + "<br>" );
    }

    return body;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEnsembleFractureStatistics::defineEditorAttribute( const caf::PdmFieldHandle* field,
                                                           QString                    uiConfigName,
                                                           caf::PdmUiEditorAttribute* attribute )
{
    if ( field == &m_filePathsTable )
    {
        auto myAttr = dynamic_cast<caf::PdmUiTextEditorAttribute*>( attribute );
        if ( myAttr )
        {
            myAttr->wrapMode = caf::PdmUiTextEditorAttribute::NoWrap;
            myAttr->textMode = caf::PdmUiTextEditorAttribute::HTML;
        }
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEnsembleFractureStatistics::fieldChangedByUi( const caf::PdmFieldHandle* changedField,
                                                      const QVariant&            oldValue,
                                                      const QVariant&            newValue )
{
    if ( changedField == &m_computeStatistics )
    {
        m_computeStatistics            = false;
        std::vector<QString> filePaths = computeStatistics();
        RicNewStimPlanFractureTemplateFeature::createNewTemplatesFromFiles( filePaths );
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEnsembleFractureStatistics::loadAndUpdateData()
{
    m_filePathsTable = generateFilePathsTable();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::vector<QString> RimEnsembleFractureStatistics::computeStatistics()
{
    auto unitSystem = RiaDefines::EclipseUnitSystem::UNITS_METRIC;

    std::vector<cvf::ref<RigStimPlanFractureDefinition>> stimPlanFractureDefinitions =
        readFractureDefinitions( m_filePaths.v(), unitSystem );

    std::set<std::pair<QString, QString>> availableResults = findAllResultNames( stimPlanFractureDefinitions );

    std::map<std::pair<RimEnsembleFractureStatistics::StatisticsType, QString>, std::shared_ptr<RigSlice2D>> statisticsGridsAll;

    // TODO: take from an incoming xml?
    double timeStep = 1.0;

    double referenceDepth = 0.0;
    for ( auto definition : stimPlanFractureDefinitions )
    {
        referenceDepth += computeDepthOfWellPathAtFracture( definition );
    }
    referenceDepth /= stimPlanFractureDefinitions.size();

    int numSamplesX = m_numSamplesX();
    int numSamplesY = m_numSamplesY();

    std::vector<double> gridXs( numSamplesX );
    std::vector<double> gridYs( numSamplesY );

    for ( auto result : availableResults )
    {
        RiaLogging::info( QString( "Creating statistics for result: %1" ).arg( result.first ) );

        std::vector<cvf::cref<RigFractureGrid>> fractureGrids =
            createFractureGrids( stimPlanFractureDefinitions, unitSystem, result.first );

        auto [minX, maxX, minY, maxY] = findExtentsOfGrids( fractureGrids );

        double sampleDistanceX = ( maxX - minX ) / numSamplesX;
        double sampleDistanceY = ( maxY - minY ) / numSamplesY;

        RiaLogging::info(
            QString( "Ensemble Fracture Size: X = [%1, %2] Y = [%3, %4]" ).arg( minX ).arg( maxX ).arg( minY ).arg( maxY ) );
        RiaLogging::info( QString( "Output size: %1x%2. Sampling Distance X = %3 Sampling Distance Y = %4" )
                              .arg( numSamplesX )
                              .arg( numSamplesY )
                              .arg( sampleDistanceX )
                              .arg( sampleDistanceY ) );

        for ( int y = 0; y < numSamplesY; y++ )
        {
            double posY = minY + y * sampleDistanceY + sampleDistanceY * 0.5;
            gridYs[y]   = referenceDepth - posY;
        }

        for ( int x = 0; x < numSamplesX; x++ )
        {
            double posX = minX + x * sampleDistanceX + sampleDistanceX * 0.5;
            gridXs[x]   = posX;
        }

        std::vector<std::vector<double>> samples( numSamplesX * numSamplesY );
        sampleAllGrids( fractureGrids, samples, minX, minY, numSamplesX, numSamplesY, sampleDistanceX, sampleDistanceY );

        std::map<RimEnsembleFractureStatistics::StatisticsType, std::shared_ptr<RigSlice2D>> statisticsGrids;
        generateStatisticsGrids( samples, numSamplesX, numSamplesY, statisticsGrids );

        for ( auto [statType, slice] : statisticsGrids )
        {
            auto key                = std::make_pair( statType, result.first );
            statisticsGridsAll[key] = slice;
        }
    }

    std::vector<QString> xmlFilePaths;

    // Save images in snapshot catalog relative to project directory
    RiaApplication* app                 = RiaApplication::instance();
    QString         outputDirectoryPath = app->createAbsolutePathFromProjectRelativePath( "fracturestats" );
    QDir            outputDirectory( outputDirectoryPath );
    if ( !outputDirectory.exists() )
    {
        outputDirectory.mkpath( outputDirectoryPath );
    }

    for ( size_t i = 0; i < caf::AppEnum<RimEnsembleFractureStatistics::StatisticsType>::size(); ++i )
    {
        caf::AppEnum<RimEnsembleFractureStatistics::StatisticsType> t =
            caf::AppEnum<RimEnsembleFractureStatistics::StatisticsType>::fromIndex( i );
        QString text = t.text();

        // Get the all the properties for this statistics type
        std::vector<std::shared_ptr<RigSlice2D>> statisticsSlices;
        std::vector<std::pair<QString, QString>> properties;
        for ( auto result : availableResults )
        {
            properties.push_back( result );
            std::shared_ptr<RigSlice2D> slice = statisticsGridsAll[std::make_pair( t.value(), result.first )];
            statisticsSlices.push_back( slice );

            QString csvFilePath = outputDirectoryPath + "/" + text + "-" + result.first + ".csv";
            writeStatisticsToCsv( csvFilePath, *slice );
        }

        QString xmlFilePath = outputDirectoryPath + "/" + name() + "-" + text + ".xml";

        RiaLogging::info( QString( "Writing fracture group statistics to: %1" ).arg( xmlFilePath ) );
        RifEnsembleFractureStatisticsExporter::writeAsStimPlanXml( statisticsSlices,
                                                                   properties,
                                                                   xmlFilePath,
                                                                   gridXs,
                                                                   gridYs,
                                                                   timeStep,
                                                                   unitSystem );

        xmlFilePaths.push_back( xmlFilePath );
    }

    return xmlFilePaths;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::vector<cvf::ref<RigStimPlanFractureDefinition>>
    RimEnsembleFractureStatistics::readFractureDefinitions( const std::vector<caf::FilePath>& filePaths,
                                                            RiaDefines::EclipseUnitSystem     unitSystem ) const
{
    double conductivityScaleFactor = 1.0;

    std::vector<cvf::ref<RigStimPlanFractureDefinition>> results;
    for ( auto filePath : m_filePaths.v() )
    {
        RiaLogging::info( QString( "Loading file: %1" ).arg( filePath.path() ) );
        QString                                 errorMessage;
        cvf::ref<RigStimPlanFractureDefinition> stimPlanFractureDefinitionData =
            RifStimPlanXmlReader::readStimPlanXMLFile( filePath.path(),
                                                       conductivityScaleFactor,
                                                       RifStimPlanXmlReader::MirrorMode::MIRROR_AUTO,
                                                       unitSystem,
                                                       &errorMessage );
        if ( !errorMessage.isEmpty() )
        {
            RiaLogging::error( QString( "Error when reading file: '%1'" ).arg( errorMessage ) );
        }

        if ( stimPlanFractureDefinitionData.notNull() )
        {
            results.push_back( stimPlanFractureDefinitionData );
        }
    }

    return results;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::set<std::pair<QString, QString>> RimEnsembleFractureStatistics::findAllResultNames(
    const std::vector<cvf::ref<RigStimPlanFractureDefinition>>& stimPlanFractureDefinitions )
{
    std::set<std::pair<QString, QString>> resultNames;
    for ( auto stimPlanFractureDefinitionData : stimPlanFractureDefinitions )
    {
        for ( auto propertyNameWithUnit : stimPlanFractureDefinitionData->getStimPlanPropertyNamesUnits() )
        {
            resultNames.insert( propertyNameWithUnit );
        }
    }

    return resultNames;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::vector<cvf::cref<RigFractureGrid>> RimEnsembleFractureStatistics::createFractureGrids(
    const std::vector<cvf::ref<RigStimPlanFractureDefinition>>& stimPlanFractureDefinitions,
    RiaDefines::EclipseUnitSystem                               unitSystem,
    const QString&                                              resultNameOnFile )
{
    // Defaults to avoid scaling
    double halfLengthScaleFactor = 1.0;
    double heightScaleFactor     = 1.0;

    std::vector<cvf::cref<RigFractureGrid>> fractureGrids;
    for ( auto stimPlanFractureDefinitionData : stimPlanFractureDefinitions )
    {
        double wellPathDepthAtFracture = computeDepthOfWellPathAtFracture( stimPlanFractureDefinitionData );

        // Always use last time steps
        std::vector<double> timeSteps           = stimPlanFractureDefinitionData->timeSteps();
        int                 activeTimeStepIndex = timeSteps.size() - 1;

        cvf::cref<RigFractureGrid> fractureGrid =
            stimPlanFractureDefinitionData->createFractureGrid( resultNameOnFile,
                                                                activeTimeStepIndex,
                                                                halfLengthScaleFactor,
                                                                heightScaleFactor,
                                                                wellPathDepthAtFracture,
                                                                unitSystem );

        if ( fractureGrid.notNull() )
        {
            fractureGrids.push_back( fractureGrid );
        }
    }

    return fractureGrids;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::tuple<double, double, double, double>
    RimEnsembleFractureStatistics::findExtentsOfGrids( const std::vector<cvf::cref<RigFractureGrid>>& fractureGrids )
{
    // Find min and max extent of all the grids
    double minX = std::numeric_limits<double>::max();
    double maxX = -std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxY = -std::numeric_limits<double>::max();

    for ( auto fractureGrid : fractureGrids )
    {
        for ( auto fractureCell : fractureGrid->fractureCells() )
        {
            for ( auto polygon : fractureCell.getPolygon() )
            {
                minX = std::min( minX, polygon.x() );
                maxX = std::max( maxX, polygon.x() );
                minY = std::min( minY, polygon.y() );
                maxY = std::max( maxY, polygon.y() );
            }
        }
    }

    return std::make_tuple( minX, maxX, minY, maxY );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimEnsembleFractureStatistics::isCoordinateInsideFractureCell( double x, double y, const RigFractureCell& cell )
{
    const cvf::Vec3d& minPoint = cell.getPolygon()[0];
    const cvf::Vec3d& maxPoint = cell.getPolygon()[2];
    // TODO: Investigate strange ordering for y coords.
    return ( x > minPoint.x() && x <= maxPoint.x() && y <= minPoint.y() && y > maxPoint.y() );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
double RimEnsembleFractureStatistics::computeDepthOfWellPathAtFracture(
    cvf::ref<RigStimPlanFractureDefinition> stimPlanFractureDefinitionData )
{
    double firstTvd = stimPlanFractureDefinitionData->topPerfTvd();
    double lastTvd  = stimPlanFractureDefinitionData->bottomPerfTvd();

    if ( firstTvd != HUGE_VAL && lastTvd != HUGE_VAL )
    {
        return ( firstTvd + lastTvd ) / 2;
    }
    else
    {
        firstTvd = stimPlanFractureDefinitionData->minDepth();
        lastTvd  = stimPlanFractureDefinitionData->maxDepth();
        return ( firstTvd + lastTvd ) / 2;
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEnsembleFractureStatistics::sampleAllGrids( const std::vector<cvf::cref<RigFractureGrid>>& fractureGrids,
                                                    std::vector<std::vector<double>>&              samples,
                                                    double                                         minX,
                                                    double                                         minY,
                                                    int                                            numSamplesX,
                                                    int                                            numSamplesY,
                                                    double                                         sampleDistanceX,
                                                    double                                         sampleDistanceY )
{
    for ( int y = 0; y < numSamplesY; y++ )
    {
        for ( int x = 0; x < numSamplesX; x++ )
        {
            double posX = minX + x * sampleDistanceX + sampleDistanceX * 0.5;
            double posY = minY + y * sampleDistanceY + sampleDistanceY * 0.5;

            for ( auto fractureGrid : fractureGrids )
            {
                for ( auto fractureCell : fractureGrid->fractureCells() )
                {
                    if ( isCoordinateInsideFractureCell( posX, posY, fractureCell ) )
                    {
                        int idx = y * numSamplesX + x;
                        samples[idx].push_back( fractureCell.getConductivityValue() );
                        break;
                    }
                }
            }
        }
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimEnsembleFractureStatistics::writeStatisticsToCsv( const QString& filePath, const RigSlice2D& samples )
{
    QFile data( filePath );
    if ( !data.open( QFile::WriteOnly | QFile::Truncate ) )
    {
        return false;
    }

    QTextStream              stream( &data );
    QString                  fieldSeparator = RiaPreferences::current()->csvTextExportFieldSeparator;
    RifCsvDataTableFormatter formatter( stream, fieldSeparator );

    std::vector<RifTextDataTableColumn> header;
    for ( size_t y = 0; y < samples.ny(); y++ )
        header.push_back( RifTextDataTableColumn( "", RifTextDataTableDoubleFormat::RIF_FLOAT ) );
    formatter.header( header );

    for ( size_t y = 0; y < samples.ny(); y++ )
    {
        for ( size_t x = 0; x < samples.nx(); x++ )
        {
            formatter.add( samples.getValue( x, y ) );
        }
        formatter.rowCompleted();
    }
    formatter.tableCompleted();
    return true;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEnsembleFractureStatistics::generateStatisticsGrids(
    const std::vector<std::vector<double>>&                                               samples,
    int                                                                                   numSamplesX,
    int                                                                                   numSamplesY,
    std::map<RimEnsembleFractureStatistics::StatisticsType, std::shared_ptr<RigSlice2D>>& statisticsGrids )
{
    for ( size_t i = 0; i < caf::AppEnum<RimEnsembleFractureStatistics::StatisticsType>::size(); ++i )
    {
        std::shared_ptr<RigSlice2D> grid = std::make_shared<RigSlice2D>( numSamplesX, numSamplesY );

        caf::AppEnum<RimEnsembleFractureStatistics::StatisticsType> t =
            caf::AppEnum<RimEnsembleFractureStatistics::StatisticsType>::fromIndex( i );
        statisticsGrids[t.value()] = grid;
    }

    for ( int y = 0; y < numSamplesY; y++ )
    {
        for ( int x = 0; x < numSamplesX; x++ )
        {
            size_t idx = y * numSamplesX + x;
            double min;
            double max;
            double sum;
            double range;
            double mean;
            double dev;
            RigStatisticsMath::calculateBasicStatistics( samples[idx], &min, &max, &sum, &range, &mean, &dev );

            statisticsGrids[RimEnsembleFractureStatistics::StatisticsType::MEAN]->setValue( x, y, mean );
            statisticsGrids[RimEnsembleFractureStatistics::StatisticsType::MIN]->setValue( x, y, min );
            statisticsGrids[RimEnsembleFractureStatistics::StatisticsType::MAX]->setValue( x, y, max );

            double p10;
            double p50;
            double p90;
            RigStatisticsMath::calculateStatisticsCurves( samples[idx], &p10, &p50, &p90, &mean );

            statisticsGrids[RimEnsembleFractureStatistics::StatisticsType::P10]->setValue( x, y, p10 );
            statisticsGrids[RimEnsembleFractureStatistics::StatisticsType::P50]->setValue( x, y, p50 );
            statisticsGrids[RimEnsembleFractureStatistics::StatisticsType::P90]->setValue( x, y, p90 );

            statisticsGrids[RimEnsembleFractureStatistics::StatisticsType::OCCURRENCE]->setValue( x, y, samples[idx].size() );
        }
    }
}