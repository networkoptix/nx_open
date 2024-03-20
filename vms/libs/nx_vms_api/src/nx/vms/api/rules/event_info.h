// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonValue>
#include <QtCore/QMap>

#include <nx/utils/uuid.h>

#include "../data/data_macros.h"

namespace nx::vms::api::rules {

struct NX_VMS_API EventInfo
{
    /**%apidoc[opt] VMS Rule ID. */
    nx::Uuid ruleId;

    /**%apidoc Event properties. */
    QMap<QString, QJsonValue> props;
};

#define nx_vms_api_rules_EventInfo_Fields \
    (ruleId)(props)

NX_VMS_API_DECLARE_STRUCT_EX(EventInfo, (json)(ubjson))

} // namespace nx::vms::api::rules
