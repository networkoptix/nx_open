// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "field.h"

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <chrono>

namespace nx::vms::api::rules {

struct NX_VMS_API ActionBuilder
{
    Q_GADGET

public:
    QnUuid id;
    QString actionType;
    QList<Field> fields;
};

#define nx_vms_api_rules_ActionBuilder_Fields \
    (id)(actionType)(fields)

QN_FUSION_DECLARE_FUNCTIONS(ActionBuilder, (json)(ubjson)(xml), NX_VMS_API)

} // namespace nx::vms::api::rules
