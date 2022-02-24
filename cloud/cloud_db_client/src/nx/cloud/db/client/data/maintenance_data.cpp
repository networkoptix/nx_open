// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "maintenance_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::cloud::db::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(VmsConnectionData, (json), VmsConnectionData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Statistics, (json), Statistics_Fields)

} // namespace nx::cloud::db::api
