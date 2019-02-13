#pragma once

#include <QJsonValue>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api::rules {

struct NX_VMS_API EventInfo
{
    Q_GADGET

public:
    QnUuid id;
    QString eventType;
    // TODO: #spanasenko EventState
    QMap<QString, QJsonValue> props;
};

#define nx_vms_api_rules_EventInfo_Fields \
    (id)(eventType)(props)

QN_FUSION_DECLARE_FUNCTIONS(EventInfo, (json), NX_VMS_API)

} // namespace nx::vms::api::rules
