// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <set>

#include <nx/reflect/instrument.h>
#include <nx/reflect/json.h>

namespace nx::vms::api {

struct Notification
{
    QnUuid id;
    QString caption;
    QString description;
    QString tooltip;
    bool acknowledge{false};
    QnUuid cameraId;
    QnUuid ruleId;
    std::chrono::microseconds timestampUs;
};
#define Notification_Fields \
    (id)(caption)(description)(tooltip)(acknowledge)(cameraId)(ruleId)(timestampUs)

NX_REFLECTION_INSTRUMENT(Notification, Notification_Fields)

struct CloudNotificationRequest
{
    QString systemId;
    std::set<QString> targets;
    Notification notification;
};
#define CloudNotificationRequest_Fields (systemId)(targets)(notification)

NX_REFLECTION_INSTRUMENT(CloudNotificationRequest, CloudNotificationRequest_Fields)

struct CloudNotification
{
    QString type;
    QString systemId;
    Notification notification;
};
#define CloudNotification_Fields (type)(systemId)(notification)

NX_REFLECTION_INSTRUMENT(CloudNotification, CloudNotification_Fields)

} // namespace nx::vms::api
