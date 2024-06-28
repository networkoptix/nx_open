// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/vms/client/core/system_finder/system_description_fwd.h>

#include "logon_data.h"

namespace nx::vms::client::core {

/** Parameters for a connection to the system with the specified cloud system id. */
NX_VMS_CLIENT_CORE_API std::optional<LogonData> cloudLogonData(const QString& systemId);

/** Parameters for a connection to the system with the specified cloud system. */
NX_VMS_CLIENT_CORE_API std::optional<LogonData> cloudLogonData(
    const SystemDescriptionPtr& system);

/** Parameters for a connection to the local system with specified url and credentials. */
NX_VMS_CLIENT_CORE_API LogonData localLogonData(
    const nx::utils::Url& url,
    const nx::network::http::Credentials& credentials);

} // namespace nx::vms::client::core
