#pragma once

#include "field.h"

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <chrono>

namespace nx::vms::api::rules {

struct NX_VMS_API ActionExecutor
{
    Q_GADGET

public:
    QnUuid id;
    QString actionType;
    QList<QList<Field>> fieldBlocks;
    std::chrono::seconds interval;
};

#define nx_vms_api_rules_ActionExecutor_Fields \
    (id)(actionType)(fieldBlocks)(interval)

QN_FUSION_DECLARE_FUNCTIONS(ActionExecutor, (json), NX_VMS_API)

} // namespace nx::vms::api::rules
