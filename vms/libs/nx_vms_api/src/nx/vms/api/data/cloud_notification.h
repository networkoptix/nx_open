// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <set>

#include <nx/reflect/instrument.h>
#include <nx/reflect/json.h>

namespace nx::vms::api {

struct Notification
{
    QString caption;
    QString description;
    QString tooltip;
    bool acknowledge{false};
    QnUuid cameraId;
    QnUuid ruleId;
    std::chrono::microseconds timestampUs;
};
#define Notification_Fields \
    (caption)(description)(tooltip)(acknowledge)(cameraId)(ruleId)(timestampUs)

NX_REFLECTION_INSTRUMENT(Notification, Notification_Fields)

struct CloudNotificationRequest
{
    QString systemId;
    std::set<QString> targets;
    Notification notification;
};
#define CloudNotificationRequest_Fields (systemId)(targets)(notification)

NX_REFLECTION_INSTRUMENT(CloudNotificationRequest, CloudNotificationRequest_Fields)

} // namespace nx::vms::api
