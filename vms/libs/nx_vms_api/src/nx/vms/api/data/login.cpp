// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "login.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LoginUserFilter, (json), LoginUserFilter_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LoginUser, (json), LoginUser_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LoginSessionRequest, (json), LoginSessionRequest_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LoginSessionFilter, (json), LoginSessionFilter_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LoginSession, (json), LoginSession_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CloudSignature, (json), CloudSignature_Fields)

} // namespace nx::vms::api
