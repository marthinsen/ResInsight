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

#pragma once
#include "cafPdmObjectCapability.h"

#include <QString>

#include <map>

namespace caf
{
class PdmObject;
class PdmObjectHandle;
class PdmObjectFactory;
class PdmScriptIOMessages;
} // namespace caf

class QTextStream;

//==================================================================================================
//
//
//
//==================================================================================================
class RicfObjectCapability : public caf::PdmObjectCapability
{
public:
    RicfObjectCapability( caf::PdmObjectHandle* owner, bool giveOwnership );

    ~RicfObjectCapability() override;

    void readFields( QTextStream&              inputStream,
                     caf::PdmObjectFactory*    objectFactory,
                     caf::PdmScriptIOMessages* errorMessageContainer );
    void writeFields( QTextStream& outputStream ) const;

private:
    caf::PdmObjectHandle* m_owner;
};