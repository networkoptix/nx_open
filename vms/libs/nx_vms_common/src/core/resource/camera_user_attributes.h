// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QSharedPointer>

#include <core/misc/schedule_task.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx/vms/api/data/dewarping_data.h>

#include "motion_window.h"

/**
 * Contains camera settings which usually can be modified by the user.
 */
class NX_VMS_COMMON_API QnCameraUserAttributes
{
public:
    QnUuid cameraId;
    nx::vms::api::MotionType motionType = nx::vms::api::MotionType::default_;
    QList<QnMotionRegion> motionRegions;
    bool licenseUsed = false;
    bool audioEnabled = false;
    bool cameraControlDisabled = false; //< TODO: #sivanov Double negation.
    QnScheduleTaskList scheduleTasks;
    bool disableDualStreaming = false; //< TODO: #sivanov Double negation.

    /**%apidoc Minimum seconds to keep camera footage. Any negative value means 'auto'. */
    std::chrono::seconds minPeriodS = -nx::vms::api::kDefaultMinArchivePeriod;

    /**%apidoc Maximum seconds to keep camera footage. Any negative value means 'auto'. */
    std::chrono::seconds maxPeriodS = -nx::vms::api::kDefaultMaxArchivePeriod;

    QnUuid preferredServerId;

    /**
     * User-given name of camera (can be different from the resource name). This is name shown to
     * the user.
     */
    QString name;
    QString groupName;
    nx::vms::api::dewarping::MediaData dewarpingParams;
    nx::vms::api::FailoverPriority failoverPriority = nx::vms::api::FailoverPriority::medium;
    nx::vms::api::CameraBackupQuality backupQuality =
        nx::vms::api::CameraBackupQuality::CameraBackup_Default;
    QString logicalId;
    int recordBeforeMotionSec = nx::vms::api::kDefaultRecordBeforeMotionSec;
    int recordAfterMotionSec = nx::vms::api::kDefaultRecordAfterMotionSec;
    nx::vms::api::BackupContentTypes backupContentType = nx::vms::api::BackupContentType::archive;
    nx::vms::api::BackupPolicy backupPolicy = nx::vms::api::BackupPolicy::byDefault;

    void assign(const QnCameraUserAttributes& right, QSet<QByteArray>* const modifiedFields);
};

using QnCameraUserAttributesPtr = QSharedPointer<QnCameraUserAttributes>;
using QnCameraUserAttributesList = QList<QnCameraUserAttributesPtr>;

Q_DECLARE_METATYPE(QnCameraUserAttributes)
Q_DECLARE_METATYPE(QnCameraUserAttributesPtr)
Q_DECLARE_METATYPE(QnCameraUserAttributesList)
