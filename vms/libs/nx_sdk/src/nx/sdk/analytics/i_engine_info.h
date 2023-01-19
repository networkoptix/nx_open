// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

namespace nx::sdk::analytics {

/** Provides various information about an Engine. */
class IEngineInfo: public Interface<IEngineInfo>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::IEngineInfo"); }

    /** @return Id assigned to this Engine by the Server. */
    virtual const char* id() const = 0;

    /** @return Name assigned to this Engine by the Server or by the user. */
    virtual const char* name() const = 0;
};
using IEngineInfo0 = IEngineInfo;

} // namespace nx::sdk::analytics
