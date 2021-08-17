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

#include "RimWellIASettings.h"

#include "RiaApplication.h"
#include "RiaPreferencesGeoMech.h"

#include "RigWellPath.h"

#include "RimDoubleParameter.h"
#include "RimGenericParameter.h"
#include "RimGeoMechCase.h"
#include "RimIntegerParameter.h"
#include "RimParameterGroup.h"
#include "RimProject.h"
#include "RimStringParameter.h"
#include "RimTools.h"
#include "RimWellPath.h"

#include "RifParameterXmlReader.h"

#include "cafPdmFieldCvfVec3d.h"
#include "cafPdmFieldScriptingCapability.h"
#include "cafPdmObjectScriptingCapability.h"
#include "cafPdmUiComboBoxEditor.h"
#include "cafPdmUiDoubleSliderEditor.h"
#include "cafPdmUiFilePathEditor.h"
#include "cafPdmUiTableViewEditor.h"

#include <QFileInfo>

#include <cmath>

CAF_PDM_SOURCE_INIT( RimWellIASettings, "RimWellIASettings" );

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimWellIASettings::RimWellIASettings()
{
    CAF_PDM_InitObject( "Integrity Analysis Model Settings", ":/WellIntAnalysis.png", "", "" );

    setName( "Model" );

    CAF_PDM_InitFieldNoDefault( &m_geomechCase, "GeomechCase", "GeoMech Case", "", "", "" );

    CAF_PDM_InitFieldNoDefault( &m_baseDir, "BaseDir", "Working Directory", "", "", "" );
    m_baseDir.uiCapability()->setUiReadOnly( true );

    CAF_PDM_InitField( &m_startMD, "StartMeasuredDepth", 0.0, "Start MD", "", "", "" );
    CAF_PDM_InitField( &m_endMD, "EndMeasuredDepth", 0.0, "End MD", "", "", "" );
    m_startMD.uiCapability()->setUiEditorTypeName( caf::PdmUiDoubleSliderEditor::uiEditorTypeName() );
    m_endMD.uiCapability()->setUiEditorTypeName( caf::PdmUiDoubleSliderEditor::uiEditorTypeName() );

    CAF_PDM_InitField( &m_bufferXY, "BufferXY", 10.0, "Model Size (XY)", "", "", "" );
    CAF_PDM_InitField( &m_bufferZ, "BufferZ", 10.0, "Depth buffer size", "", "", "" );

    CAF_PDM_InitFieldNoDefault( &m_parameters, "ModelingParameters", "Modeling Parameters", ":/Bullet.png", "", "" );

    CAF_PDM_InitFieldNoDefault( &m_nameProxy, "NameProxy", "Name Proxy", "", "", "" );
    m_nameProxy.registerGetMethod( this, &RimWellIASettings::fullName );
    m_nameProxy.uiCapability()->setUiReadOnly( true );
    m_nameProxy.uiCapability()->setUiHidden( true );
    m_nameProxy.xmlCapability()->disableIO();

    this->setDeletable( true );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimWellIASettings::~RimWellIASettings()
{
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimWellIASettings::initSettings( QString& outErrmsg )
{
    initResInsightParameters();

    RifParameterXmlReader basicreader( RiaPreferencesGeoMech::current()->geomechWIADefaultXML() );
    if ( !basicreader.parseFile( outErrmsg ) ) return false;

    m_parameters.clear();
    for ( auto group : basicreader.parameterGroups() )
    {
        m_parameters.push_back( group );
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimWellIASettings::fieldChangedByUi( const caf::PdmFieldHandle* changedField,
                                          const QVariant&            oldValue,
                                          const QVariant&            newValue )
{
    if ( ( changedField == &m_startMD ) || ( changedField == &m_endMD ) || ( changedField == objectToggleField() ) ||
         ( changedField == &m_bufferXY ) || ( changedField == &m_bufferZ ) )
    {
        generateModelBox();

        RiaApplication::instance()->project()->scheduleCreateDisplayModelAndRedrawAllViews();
    }

    this->updateConnectedEditors();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QList<caf::PdmOptionItemInfo> RimWellIASettings::calculateValueOptions( const caf::PdmFieldHandle* fieldNeedingOptions,
                                                                        bool*                      useOptionsOnly )
{
    QList<caf::PdmOptionItemInfo> options;

    if ( fieldNeedingOptions == &m_geomechCase )
    {
        RimTools::geoMechCaseOptionItems( &options );
    }

    return options;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimWellIASettings::defineUiOrdering( QString uiConfigName, caf::PdmUiOrdering& uiOrdering )
{
    RimWellPath* wellPath;
    firstAncestorOrThisOfType( wellPath );
    if ( wellPath )
    {
        if ( wellPath->unitSystem() == RiaDefines::EclipseUnitSystem::UNITS_METRIC )
        {
            m_startMD.uiCapability()->setUiName( "Start MD [m]" );
            m_endMD.uiCapability()->setUiName( "End MD [m]" );
        }
        else if ( wellPath->unitSystem() == RiaDefines::EclipseUnitSystem::UNITS_FIELD )
        {
            m_startMD.uiCapability()->setUiName( "Start MD [ft]" );
            m_endMD.uiCapability()->setUiName( "End MD [ft]" );
        }
    }

    uiOrdering.add( nameField() );
    uiOrdering.add( &m_geomechCase );
    uiOrdering.add( &m_baseDir );
    uiOrdering.add( &m_startMD );
    uiOrdering.add( &m_endMD );
    uiOrdering.add( &m_bufferXY );
    uiOrdering.add( &m_bufferZ );

    uiOrdering.skipRemainingFields( true );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimWellIASettings::defineEditorAttribute( const caf::PdmFieldHandle* field,
                                               QString                    uiConfigName,
                                               caf::PdmUiEditorAttribute* attribute )
{
    if ( field == &m_startMD || field == &m_endMD )
    {
        caf::PdmUiDoubleSliderEditorAttribute* myAttr = dynamic_cast<caf::PdmUiDoubleSliderEditorAttribute*>( attribute );

        if ( myAttr )
        {
            RimWellPath* wellPath = nullptr;
            this->firstAncestorOrThisOfType( wellPath );
            if ( !wellPath ) return;

            myAttr->m_minimum = wellPath->uniqueStartMD();
            myAttr->m_maximum = wellPath->uniqueEndMD();
        }
    }
}

//--------------------------------------------------------------------------------------------------
/// Return the name to show in the tree selector
//--------------------------------------------------------------------------------------------------
QString RimWellIASettings::fullName() const
{
    return QString( "%1 - [%2 - %3]" ).arg( name() ).arg( m_startMD ).arg( m_endMD );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
caf::PdmFieldHandle* RimWellIASettings::userDescriptionField()
{
    return &m_nameProxy;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QString RimWellIASettings::geomechCaseFilename() const
{
    if ( m_geomechCase ) return m_geomechCase->gridFileName();
    return "";
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QString RimWellIASettings::geomechCaseName() const
{
    QFileInfo fi( geomechCaseFilename() );
    return fi.baseName();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimGeoMechCase* RimWellIASettings::geomechCase() const
{
    return m_geomechCase;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QString RimWellIASettings::outputBaseDirectory() const
{
    return m_baseDir();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QString RimWellIASettings::modelInputFilename() const
{
    return m_baseDir() + "/model_input.json";
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimWellIASettings::setGeoMechCase( RimGeoMechCase* geomechCase )
{
    m_geomechCase = geomechCase;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimWellIASettings::setOutputBaseDirectory( QString baseDir )
{
    m_baseDir = baseDir;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::list<RimParameterGroup*> RimWellIASettings::inputParameterGroups()
{
    std::list<RimParameterGroup*> retlist;

    for ( auto& group : m_parameters )
        retlist.push_back( group );

    return retlist;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::list<RimParameterGroup*> RimWellIASettings::resinsightParameterGroups()
{
    std::list<RimParameterGroup*> retlist;

    for ( auto& group : m_parametersRI )
        retlist.push_back( group );

    return retlist;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimWellIASettings::initResInsightParameters()
{
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimWellIASettings::updateResInsightParameters()
{
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimGenericParameter* RimWellIASettings::getInputParameter( QString name ) const
{
    RimGenericParameter* retval = nullptr;

    for ( auto group : m_parameters.childObjects() )
    {
        retval = group->parameter( name );
        if ( retval != nullptr ) return retval;
    }

    return retval;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimWellIASettings::setDepthInterval( double startMD, double endMD )
{
    m_startMD = startMD;
    m_endMD   = endMD;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
double RimWellIASettings::startMD()
{
    return m_startMD;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
double RimWellIASettings::endMD()
{
    return m_endMD;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimWellPath* RimWellIASettings::wellPath() const
{
    RimWellPath* wellpath = nullptr;
    this->firstAncestorOrThisOfTypeAsserted( wellpath );

    return wellpath;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimWellIASettings::generateModelBox()
{
    RimWellPath* path = wellPath();

    RigWellPath* wellgeom = path->wellPathGeometry();

    cvf::Vec3d startPos = wellgeom->interpolatedPointAlongWellPath( m_startMD );
    cvf::Vec3d endPos   = wellgeom->interpolatedPointAlongWellPath( m_endMD );
}
