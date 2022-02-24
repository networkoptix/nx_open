// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "field.h"

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api::rules {

struct NX_VMS_API EventFilter
{
    Q_GADGET

public:
    QnUuid id;
    QString eventType;
    // TODO: #spanasenko EventState.
    QList<Field> fields;
    // TODO: #spanasenko Custom Field blocks.
};

#define nx_vms_api_rules_EventFilter_Fields \
    (id)(eventType)(fields)

QN_FUSION_DECLARE_FUNCTIONS(EventFilter, (json)(ubjson)(xml), NX_VMS_API)

} // namespace nx::vms::api::rules
