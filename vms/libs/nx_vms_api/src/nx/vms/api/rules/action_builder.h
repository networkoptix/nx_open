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
    std::chrono::seconds interval = std::chrono::seconds(0);
};

#define nx_vms_api_rules_ActionBuilder_Fields \
    (id)(actionType)(fields)(interval)

QN_FUSION_DECLARE_FUNCTIONS(ActionBuilder, (json), NX_VMS_API)

} // namespace nx::vms::api::rules
