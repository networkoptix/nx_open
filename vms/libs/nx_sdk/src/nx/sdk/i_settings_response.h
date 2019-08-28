// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

#include <nx/sdk/i_string_map.h>

namespace nx {
namespace sdk {

class ISettingsResponse: public Interface<ISettingsResponse>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::ISettingsResponse"); }

    /**
     * @return Map of setting values, indexed by setting ids.
     */
    protected: virtual IStringMap* getValues() const = 0;
    public: Ptr<IStringMap> values() const { return toPtr(getValues()); }

    /**
     * @return Map of errors that happened while obtaining setting values, indexed by setting
     *     ids. Each value must be a human-readable error message in English.
     */
    protected: virtual IStringMap* getErrors() const = 0;
    public: Ptr<IStringMap> errors() const { return toPtr(getErrors()); }
};

} // namespace sdk
} // namespace nx
