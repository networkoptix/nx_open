// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "email_settings.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EmailSettings, (json), EmailSettings_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EmailSettingsRequest, (json), EmailSettingsRequest_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EmailSettingsReply, (json), EmailSettingsReply_Fields)

} // namespace nx::vms::api
