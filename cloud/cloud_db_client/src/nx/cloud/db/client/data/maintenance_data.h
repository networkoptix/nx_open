// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/cloud/db/api/maintenance_manager.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/reflect/instrument.h>

namespace nx::cloud::db::api {

#define VmsConnectionData_Fields (systemId)(endpoint)

NX_REFLECTION_INSTRUMENT(VmsConnectionData, VmsConnectionData_Fields)

#define Statistics_Fields (onlineServerCount)

NX_REFLECTION_INSTRUMENT(Statistics, Statistics_Fields)

QN_FUSION_DECLARE_FUNCTIONS(VmsConnectionData, (json))
QN_FUSION_DECLARE_FUNCTIONS(Statistics, (json))

} // namespace nx::cloud::db::api
