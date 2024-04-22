// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonValue>
#include <QtCore/QMap>

#include <nx/utils/uuid.h>

#include "../data/data_macros.h"

namespace nx::vms::api::rules {

struct NX_VMS_API ActionInfo
{
    /**%apidoc[opt] Id of the executed rule. */
    nx::Uuid ruleId;

    /**%apidoc Action properties. */
    QMap<QString, QJsonValue> props;
};
#define nx_vms_api_rules_ActionInfo_Fields \
    (ruleId)(props)

NX_VMS_API_DECLARE_STRUCT_EX(ActionInfo, (json)(ubjson))

} // namespace nx::vms::api::rules
