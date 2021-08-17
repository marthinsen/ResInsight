/////////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2021  Equinor ASA
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

#include "RifWellIAJsonWriter.h"

#include "RimGenericParameter.h"
#include "RimParameterGroup.h"
#include "RimParameterGroups.h"
#include "RimWellIASettings.h"

#include <QFile>
#include <QTextStream>

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RifWellIAJsonWriter::writeToParameterFile( RimWellIASettings& settings, QString& outErrorText )
{
    QString filename = settings.modelInputFilename();

    outErrorText = "Unable to write to file \"" + filename + "\" - ";

    QFile file( filename );
    if ( file.open( QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text ) )
    {
        QTextStream stream( &file );

        stream << "{" << endl;
        stream << "\"comments\": \"All units are SI unless mentioned otherwise; temperature is in Celcius; use forward "
                  "slash (/) in 'directory' definition\","
               << endl;
        stream << "\"directory\": \"" + settings.outputBaseDirectory() + "\"," << endl;
        stream << "\"output_name\": \"" + settings.name() + "\"";

        RimParameterGroups mergedGroups;

        bool mergeInCommentParameter = true;

        for ( auto& group : settings.inputParameterGroups() )
        {
            mergedGroups.mergeGroup( group, mergeInCommentParameter );
        }
        for ( auto& group : settings.resinsightParameterGroups() )
        {
            mergedGroups.mergeGroup( group, mergeInCommentParameter );
        }

        for ( auto& group : mergedGroups.groups() )
        {
            stream << "," << endl;

            stream << "\"" + group->name() + "\": {" << endl;

            const auto& parameters = group->parameters();

            for ( int i = 0; i < parameters.size(); )
            {
                stream << "   \"" + parameters[i]->name() + "\": " + parameters[i]->jsonValue();

                i++;
                if ( i < parameters.size() )
                {
                    stream << ",";
                }
                stream << endl;
            }

            stream << "   }";
        }

        stream << endl << "}" << endl;
        file.close();
    }
    else
    {
        outErrorText += "Could not open file.";
        return false;
    }

    outErrorText = "";
    return true;
}
