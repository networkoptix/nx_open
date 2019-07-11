// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

#include <nx/sdk/i_string_map.h>

namespace nx {
namespace sdk {

class ISettingsResponse: public Interface<ISettingsResponse>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::ISettingsResponse"); }

    /**
     * @return map of settings values having setting ids as keys.
     */
    virtual IStringMap* values() const = 0;

    /**
     * @return map of errors that happened while obtaining setting values. Keys are setting ids,
     *     values are human-readable error strings.
     */
    virtual IStringMap* errors() const = 0;
};

} // namespace sdk
} // namespace nx
