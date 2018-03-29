#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QSharedPointer>

#include <core/ptz/media_dewarping_params.h>
#include <core/misc/schedule_task.h>

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
    Qn::MotionType motionType = Qn::MT_Default;
    QList<QnMotionRegion> motionRegions;
    bool licenseUsed = false;
    bool audioEnabled = false;
    bool cameraControlDisabled = false; //< TODO: #GDM Double negation.
    QnScheduleTaskList scheduleTasks;
    bool disableDualStreaming = false; //< TODO: #GDM Double negation.
    int minDays = 0;
    int maxDays = 0;
    QnUuid preferredServerId;
    //!User-given name of camera (can be different from resource name). This is name shown to the user
    QString name;
    QString groupName;
    QnMediaDewarpingParams dewarpingParams;
    Qn::FailoverPriority failoverPriority = Qn::FP_Medium;
    Qn::CameraBackupQualities backupQualities = Qn::CameraBackup_Default;
    QString logicalId;

    void assign(const QnCameraUserAttributes& right, QSet<QByteArray>* const modifiedFields);
};

Q_DECLARE_METATYPE(QnCameraUserAttributes)
Q_DECLARE_METATYPE(QnCameraUserAttributesPtr)
Q_DECLARE_METATYPE(QnCameraUserAttributesList)
