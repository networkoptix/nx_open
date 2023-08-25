// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/i_string.h>
#include <nx/sdk/i_string_map.h>
#include <nx/sdk/interface.h>

namespace nx::sdk {

class ISettingsResponse0: public Interface<ISettingsResponse0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::ISettingsResponse"); }

    /** Called by values() */
    protected: virtual IStringMap* getValues() const = 0;
    /**
     * @return Map of setting values, indexed by setting ids. Can be null or empty if there are no
     *     setting values to return.
     */
    public: Ptr<IStringMap> values() const { return Ptr(getValues()); }

    /** Called by errors() */
    protected: virtual IStringMap* getErrors() const = 0;
    /**
     * @return Map of errors that happened while processing (applying or obtaining) setting values,
     *     indexed by setting ids. Each value must be a human-readable error message in English.
     *     Can be null or empty if there are no errors.
     */
    public: Ptr<IStringMap> errors() const { return Ptr(getErrors()); }
};

/**
 * The data to be returned from the plugin describing the desired settings.
 *
 * A combination of optional individual setting errors, optional new setting values in case they
 * were adjusted, and an optional new Settings Model.
 */
class ISettingsResponse: public Interface<ISettingsResponse, ISettingsResponse0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::ISettingsResponse1"); }

    /** Called by model() */
    protected: virtual IString* getModel() const = 0;
    /**
     * @return New Settings Model, overriding the one from the parent object's Manifest, in case
     *     the Model has been changed e.g. because its parts depend on some setting values. Can be
     *     null if the Model has not been changed.
     */
    public: Ptr<IString> model() const { return Ptr(getModel()); }
};
using ISettingsResponse1 = ISettingsResponse;

} // namespace nx::sdk
