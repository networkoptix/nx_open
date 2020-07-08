// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

#include <nx/sdk/i_string.h>
#include <nx/sdk/i_string_map.h>

namespace nx {
namespace sdk {

class ISettingsResponse0: public Interface<ISettingsResponse0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::ISettingsResponse"); }

    /** Called by values() */
    protected: virtual IStringMap* getValues() const = 0;
    /**
     * @return Map of setting values, indexed by setting ids.
     */
    public: Ptr<IStringMap> values() const { return toPtr(getValues()); }

    /** Called by errors() */
    protected: virtual IStringMap* getErrors() const = 0;
    /**
     * @return Map of errors that happened while obtaining setting values, indexed by setting
     *     ids. Each value must be a human-readable error message in English.
     */
    public: Ptr<IStringMap> errors() const { return toPtr(getErrors()); }
};

class ISettingsResponse: public Interface<ISettingsResponse, ISettingsResponse0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::ISettingsResponse0"); }

    /** Called by model() */
    protected: virtual IString* getModel() const = 0;
    public: Ptr<IString> model() const { return toPtr(getModel()); }
};

} // namespace sdk
} // namespace nx
