// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qr_code_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QrCodeRequestData,
    (json),
    QrCodeRequestData_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QrCodeData,
    (json),
    QrCodeData_Fields)

} // namespace nx::vms::api
