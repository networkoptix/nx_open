// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/thread/mutex.h>
#include <core/resource/general_attribute_pool.h>
#include <core/resource/camera_user_attributes.h>
#include <nx/utils/singleton.h>

/*
 * Temporary class for minimizing chages in 5.0 version. Should be deleted in master.
 * Attributes should be maintained by the resource itself.
 */

class NX_VMS_COMMON_API QnCameraUserAttributePool: public QObject
{
    Q_OBJECT
public:
    explicit QnCameraUserAttributePool(QObject* parent = nullptr);

public:
    QnUuid preferredServerId(const QnUuid& cameraId);
    bool setPreferredServerId(const QnUuid& cameraId, const QnUuid& value);

    nx::vms::api::FailoverPriority failoverPriority(const QnUuid& cameraId);
    bool setFailoverPriority(const QnUuid& cameraId, nx::vms::api::FailoverPriority value);

    bool licenseUsed(const QnUuid& cameraId);
    bool setLicenseUsed(const QnUuid& cameraId, bool value);

    bool cameraControlDisabled(const QnUuid& cameraId);
    bool setCameraControlDisabled(const QnUuid& cameraId, bool value);

    bool audioEnabled(const QnUuid& cameraId);
    bool setAudioEnabled(const QnUuid& cameraId, bool value);

    bool disableDualStreaming(const QnUuid& cameraId);
    bool setDisableDualStreaming(const QnUuid& cameraId, bool value);

    nx::vms::api::dewarping::MediaData dewarpingParams(const QnUuid& cameraId);
    bool setDewarpingParams(const QnUuid& cameraId, const nx::vms::api::dewarping::MediaData& value);

    QList<QnMotionRegion> motionRegions(const QnUuid& cameraId);
    bool setMotionRegions(const QnUuid& cameraId, const QList<QnMotionRegion>& value);

    nx::vms::api::MotionType motionType(const QnUuid& cameraId);
    bool setMotionType(const QnUuid& cameraId, nx::vms::api::MotionType value);

    QString name(const QnUuid& cameraId);
    bool setName(const QnUuid& cameraId, const QString& value);

    QString groupName(const QnUuid& cameraId);
    bool setGroupName(const QnUuid& cameraId, const QString& value);

    QString logicalId(const QnUuid& cameraId);
    bool setLogicalId(const QnUuid& cameraId, const QString& value);

    std::chrono::seconds minPeriod(const QnUuid& cameraId);
    bool setMinPeriod(const QnUuid& cameraId, std::chrono::seconds value);

    std::chrono::seconds maxPeriod(const QnUuid& cameraId);
    bool setMaxPeriod(const QnUuid& cameraId, std::chrono::seconds value);

    int recordBeforeMotionSec(const QnUuid& cameraId);
    bool setRecordBeforeMotionSec(const QnUuid& cameraId, int value);

    int recordAfterMotionSec(const QnUuid& cameraId);
    bool setRecordAfterMotionSec(const QnUuid& cameraId, int value);

    QnScheduleTaskList scheduleTasks(const QnUuid& cameraId);
    bool setScheduleTasks(const QnUuid& cameraId, QnScheduleTaskList value);

    nx::vms::api::CameraBackupQuality backupQuality(const QnUuid& cameraId);
    bool setBackupQuality(const QnUuid& cameraId, nx::vms::api::CameraBackupQuality value);

    nx::vms::api::BackupContentTypes backupContentType(const QnUuid& cameraId);
    bool setBackupContentType(const QnUuid& cameraId, nx::vms::api::BackupContentTypes value);

    nx::vms::api::BackupPolicy backupPolicy(const QnUuid& cameraId);
    bool setBackupPolicy(const QnUuid& cameraId, nx::vms::api::BackupPolicy value);

public:
    void clear();
    QnCameraUserAttributesList getAttributesList(const QList<QnUuid>& idList);
    void assign(
        const QnUuid& cameraId,
        const QnCameraUserAttributes& sourceAttributes,
        QSet<QByteArray>* const modifiedFields);

    void update(const QnUuid& cameraId, const QnCameraUserAttributes& source);
    QnCameraUserAttributesPtr getCopy(const QnUuid& cameraId);

private:
    template<typename T>
    bool setValue(const QnUuid& cameraId, T QnCameraUserAttributes::* field, const T& value)
    {
        NX_WRITE_LOCKER locker{&m_mutex};
        auto itr = m_elements.find(cameraId);

        if (itr == m_elements.cend())
        {
            auto [insertedItr, success] = m_elements.insert(std::pair{cameraId, QnCameraUserAttributes()});
            insertedItr->second.cameraId = cameraId;
            itr = insertedItr;
        }
        QnCameraUserAttributes& attributes = itr->second;
        if (attributes.*field == value)
            return false;
        attributes.*field = value;
        return true;
    }

    template<typename T>
    T getValue(const QnUuid& cameraId, T QnCameraUserAttributes::* field)
    {
        NX_READ_LOCKER locker{&m_mutex};
        if (const auto itr = m_elements.find(cameraId); itr != m_elements.cend())
            return itr->second.*field;

        const static QnCameraUserAttributes defaultValue;
        return defaultValue.*field;
    }

private:
    std::map<QnUuid, QnCameraUserAttributes> m_elements;
    nx::ReadWriteLock m_mutex;
};
