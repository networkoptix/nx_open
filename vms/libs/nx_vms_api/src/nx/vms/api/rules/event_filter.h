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
    // TODO: #spanasenko EventState
    QList<QList<Field>> fieldBlocks;
};

#define nx_vms_api_rules_EventFilter_Fields \
    (id)(eventType)(fieldBlocks)

QN_FUSION_DECLARE_FUNCTIONS(EventFilter, (json), NX_VMS_API)

} // namespace nx::vms::api::rules
