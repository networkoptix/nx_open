// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <QtCore/QJsonValue>

#include "data_macros.h"

namespace nx::vms::api {

struct CloudNotificationRequest
{
    QString systemId;
    std::set<QString> targets;
    QMap<QString, QJsonValue> notification;
};
#define CloudNotificationRequest_Fields (systemId)(targets)(notification)

NX_VMS_API_DECLARE_STRUCT_EX(CloudNotificationRequest, (json))

struct CloudNotification
{
    QString type;
    QString systemId;
    QMap<QString, QJsonValue> notification;
};
#define CloudNotification_Fields (type)(systemId)(notification)

NX_VMS_API_DECLARE_STRUCT_EX(CloudNotification, (json))

} // namespace nx::vms::api
