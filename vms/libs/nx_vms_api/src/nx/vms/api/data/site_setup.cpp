// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "site_setup.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LocalSiteAuth, (json), LocalSystemAuth_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CloudSiteAuth, (json), CloudSiteAuth_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CloudSystemAuth, (json), CloudSystemAuth_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SetupSiteData, (json), SetupSiteData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SetupSystemData, (json), SetupSystemData_Fields)

} // namespace nx::vms::api
