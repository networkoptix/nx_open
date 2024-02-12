// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::core {

/**
 * Parameters of the local or cloud connection.
 */
struct ConnectionData
{
    /** Server url with username. */
    nx::utils::Url url;

    /**
     * Id of the system. Used as the credentials storage key.
     * For the cloud systems cloud id is used, local id for others.
     */
    nx::Uuid systemId;

    bool operator==(const ConnectionData& other) const = default;

};
NX_REFLECTION_INSTRUMENT(ConnectionData, (url)(systemId));

} // namespace nx::vms::client::desktop
