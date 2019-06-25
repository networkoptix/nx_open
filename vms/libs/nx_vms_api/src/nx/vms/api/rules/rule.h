#pragma once

#include "action_executor.h"
#include "event_filter.h"

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api::rules {

struct NX_VMS_API Rule
{
    Q_GADGET

public:
    QnUuid id;
    // TODO: #spanasenko and-or logic
    QList<EventFilter> eventList;
    QList<ActionExecutor> actionList;
    bool enabled;
    QByteArray schedule;
    QString comment;
};

#define nx_vms_api_rules_Rule_Fields \
    (id)(eventList)(actionList)(enabled)(schedule)(comment)

QN_FUSION_DECLARE_FUNCTIONS(Rule, (json), NX_VMS_API)

} // namespace nx::vms::api::rules