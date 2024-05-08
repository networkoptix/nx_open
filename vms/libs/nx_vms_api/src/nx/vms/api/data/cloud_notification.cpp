// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_notification.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    CloudNotificationRequest, (json), CloudNotificationRequest_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    CloudAuthenticate, (json), CloudAuthenticate_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    CloudNotification, (json), CloudNotification_Fields)

} // namespace nx::vms::api
