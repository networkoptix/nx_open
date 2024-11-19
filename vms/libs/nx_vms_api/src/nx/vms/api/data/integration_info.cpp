// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration_info.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    IntegrationInfoRequest, (json), IntegrationInfoRequest_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    IntegrationResourceBindingInfo, (json), IntegrationResourceBindingInfo_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    IntegrationInfo, (json), IntegrationInfo_Fields)

}
