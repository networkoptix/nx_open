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
    if (backupQualities != right.backupQualities)
        *modifiedFields << "backupQualitiesChanged";
    if (logicalId != right.logicalId)
        *modifiedFields << "logicalIdChanged";
    if (audioEnabled != right.audioEnabled)
        *modifiedFields << "audioEnabledChanged";
    *this = right;
}
