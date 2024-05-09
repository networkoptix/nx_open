// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/cloud/db/api/maintenance_manager.h>
#include <nx/reflect/instrument.h>

namespace nx::cloud::db::api {

NX_REFLECTION_INSTRUMENT(VmsConnectionData, (systemId)(endpoint))
NX_REFLECTION_INSTRUMENT(Statistics, (onlineServerCount))
NX_REFLECTION_INSTRUMENT(AccountLock, (key)(lockType))

} // namespace nx::cloud::db::api
