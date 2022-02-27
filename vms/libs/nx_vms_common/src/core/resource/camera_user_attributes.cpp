// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_user_attributes.h"

#include <api/global_settings.h>

void QnCameraUserAttributes::assign(
    const QnCameraUserAttributes& right,
    QSet<QByteArray>* const modifiedFields)
{
    if (name != right.name)
        *modifiedFields << "nameChanged";
    if (groupName != right.groupName)
        *modifiedFields << "groupNameChanged";
    if (dewarpingParams != right.dewarpingParams)
        *modifiedFields << "mediaDewarpingParamsChanged";
    if (licenseUsed != right.licenseUsed)
        *modifiedFields << "licenseUsedChanged";
    if (scheduleTasks != right.scheduleTasks)
        *modifiedFields << "scheduleTasksChanged";
    if (motionRegions != right.motionRegions)
        *modifiedFields << "motionRegionChanged";
    if (failoverPriority != right.failoverPriority)
        *modifiedFields << "failoverPriorityChanged";
    if (backupQuality != right.backupQuality)
        *modifiedFields << "backupQualityChanged";
    if (logicalId != right.logicalId)
        *modifiedFields << "logicalIdChanged";
    if (audioEnabled != right.audioEnabled)
        *modifiedFields << "audioEnabledChanged";
    if (motionType != right.motionType)
        *modifiedFields << "motionTypeChanged";
    if (backupContentType != right.backupContentType)
        *modifiedFields << "backupContentTypeChanged";
    if (backupPolicy != right.backupPolicy)
        *modifiedFields << "backupPolicyChanged";
    *this = right;
}
