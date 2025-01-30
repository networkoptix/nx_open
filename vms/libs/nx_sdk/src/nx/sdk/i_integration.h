// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

namespace nx::sdk {

class IUtilityProvider;

/**
 * Base interface for derived interfaces representing various Integration types. Objects
 * implementing such interfaces are created in the plugins by exported functions declared in
 * nx/sdk/entry_points.h, and are destroyed (via releaseRef()) on the process shutdown. Such object
 * acts as a starting point for all Integration features - they are accessible via other objects
 * created by this one.
 *
 * All methods of all Integration objects and objects that they create, directly or indirectly, are
 * guaranteed to be called without overlapping even if from different threads (i.e. with a
 * guaranteed fence between the calls), thus, no synchronization is required for the
 * implementation.
 */
class IIntegration: public Interface<IIntegration>
{
public:
    static auto interfaceId()
    {
        return makeIdWithAlternative("nx::sdk::IIntegration",
            /* Old name from 6.0. */ "nx::sdk::IPlugin");
    }

    /**
     * Provides an object which the Integration can use for calling back to access some data and
     * functionality provided by the process that uses the Integration.
     *
     * For the details, see the documentation for IUtilityProvider.
     */
    virtual void setUtilityProvider(IUtilityProvider* utilityProvider) = 0;
};
using IIntegration0 = IIntegration;

} // namespace nx::sdk
