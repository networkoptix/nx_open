#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QSharedPointer>

#include <core/ptz/media_dewarping_params.h>
#include <core/misc/schedule_task.h>

#include <nx/vms/api/data/camera_attributes_data.h>

#include "motion_window.h"

#include <nx/utils/uuid.h>
//!Contains camera settings usually modified by user
/*!
    E.g., recording schedule, motion type, second stream quality, etc...
*/
class QnCameraUserAttributes
{
public:
    QnUuid cameraId;
    Qn::MotionType motionType = Qn::MotionType::MT_Default;
    QList<QnMotionRegion> motionRegions;
    bool licenseUsed = false;
    bool audioEnabled = false;
    bool cameraControlDisabled = false; //< TODO: #GDM Double negation.
    QnScheduleTaskList scheduleTasks;
    bool disableDualStreaming = false; //< TODO: #GDM Double negation.
    int minDays = -nx::vms::api::kDefaultMinArchiveDays; //< Negative means 'auto'.
    int maxDays = -nx::vms::api::kDefaultMaxArchiveDays; //< Negative means 'auto'.
    QnUuid preferredServerId;
    //!User-given name of camera (can be different from resource name). This is name shown to the user
    QString name;
    QString groupName;
    QnMediaDewarpingParams dewarpingParams;
    Qn::FailoverPriority failoverPriority = Qn::FailoverPriority::medium;
    Qn::CameraBackupQualities backupQualities = Qn::CameraBackupQuality::CameraBackup_Default;
    QString logicalId;
    int recordBeforeMotionSec = nx::vms::api::kDefaultRecordBeforeMotionSec;
    int recordAfterMotionSec = nx::vms::api::kDefaultRecordAfterMotionSec;

    void assign(const QnCameraUserAttributes& right, QSet<QByteArray>* const modifiedFields);
};

Q_DECLARE_METATYPE(QnCameraUserAttributes)
Q_DECLARE_METATYPE(QnCameraUserAttributesPtr)
Q_DECLARE_METATYPE(QnCameraUserAttributesList)
