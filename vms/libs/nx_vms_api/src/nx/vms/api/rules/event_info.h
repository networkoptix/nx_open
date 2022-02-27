// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonValue>
#include <QtCore/QMap>

#include <nx/utils/uuid.h>
#include <nx/vms/api/data/data_macros.h>

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
NX_VMS_API_DECLARE_STRUCT(EventInfo)

QN_FUSION_DECLARE_FUNCTIONS(EventInfo, (json)(ubjson), NX_VMS_API)

} // namespace nx::vms::api::rules
