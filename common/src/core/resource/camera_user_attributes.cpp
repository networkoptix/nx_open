/**********************************************************
* 2 oct 2014
* akolesnikov
***********************************************************/

#include "camera_user_attributes.h"

#include <api/global_settings.h>

#include <nx/streaming/config.h> // TODO: #GDM get rid of this dependency

QnCameraUserAttributes::QnCameraUserAttributes()
:
    motionType(Qn::MT_Default),
    scheduleDisabled(true),
    audioEnabled(false),
    cameraControlDisabled(false),
    disableDualStreaming(false),
    minDays(0),
    maxDays(0),
    failoverPriority(Qn::FP_Medium),
    backupQualities(Qn::CameraBackup_Default)
{
}

void QnCameraUserAttributes::assign( const QnCameraUserAttributes& right, QSet<QByteArray>* const modifiedFields )
{
    if( name != right.name )
        *modifiedFields << "nameChanged";
    if( groupName != right.groupName )
        *modifiedFields << "groupNameChanged";
    if( dewarpingParams != right.dewarpingParams )
        *modifiedFields << "mediaDewarpingParamsChanged";
    if( scheduleDisabled != right.scheduleDisabled )
        *modifiedFields << "scheduleDisabledChanged";
    if( scheduleTasks != right.scheduleTasks )
        *modifiedFields << "scheduleTasksChanged";
    if( motionRegions != right.motionRegions )
        *modifiedFields << "motionRegionChanged";
    if (licenseUsed != right.licenseUsed)
        *modifiedFields << "licenseUsedChanged";
    if (failoverPriority != right.failoverPriority)
        *modifiedFields << "failoverPriorityChanged";
    if (backupQualities != right.backupQualities)
        *modifiedFields << "backupQualitiesChanged";

    *this = right;
}
