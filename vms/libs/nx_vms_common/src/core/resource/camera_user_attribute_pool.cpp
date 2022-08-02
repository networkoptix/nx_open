// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_user_attribute_pool.h"

QnCameraUserAttributePool::QnCameraUserAttributePool(QObject* parent):
    QObject(parent)
{
}

QnUuid QnCameraUserAttributePool::preferredServerId(const QnUuid& cameraId)
{
    return getValue(cameraId, &QnCameraUserAttributes::preferredServerId);
}

bool QnCameraUserAttributePool::setPreferredServerId(const QnUuid& cameraId, const QnUuid& value)
{
    return setValue(cameraId, &QnCameraUserAttributes::preferredServerId, value);
}

nx::vms::api::FailoverPriority QnCameraUserAttributePool::failoverPriority(const QnUuid& cameraId)
{
    return getValue(cameraId, &QnCameraUserAttributes::failoverPriority);
}

bool QnCameraUserAttributePool::setFailoverPriority(const QnUuid& cameraId,
    nx::vms::api::FailoverPriority value)
{
    return setValue(cameraId, &QnCameraUserAttributes::failoverPriority, value);
}

bool QnCameraUserAttributePool::licenseUsed(const QnUuid& cameraId)
{
    return getValue(cameraId, &QnCameraUserAttributes::licenseUsed);
}

bool QnCameraUserAttributePool::setLicenseUsed(const QnUuid& cameraId, bool value)
{
    return setValue(cameraId, &QnCameraUserAttributes::licenseUsed, value);
}

bool QnCameraUserAttributePool::cameraControlDisabled(const QnUuid& cameraId)
{
    return getValue(cameraId, &QnCameraUserAttributes::cameraControlDisabled);
}

bool QnCameraUserAttributePool::setCameraControlDisabled(const QnUuid& cameraId, bool value)
{
    return setValue(cameraId, &QnCameraUserAttributes::cameraControlDisabled, value);
}

bool QnCameraUserAttributePool::audioEnabled(const QnUuid& cameraId)
{
    return getValue(cameraId, &QnCameraUserAttributes::audioEnabled);
}

bool QnCameraUserAttributePool::setAudioEnabled(const QnUuid& cameraId, bool value)
{
    return setValue(cameraId, &QnCameraUserAttributes::audioEnabled, value);
}

bool QnCameraUserAttributePool::disableDualStreaming(const QnUuid& cameraId)
{
    return getValue(cameraId, &QnCameraUserAttributes::disableDualStreaming);
}

bool QnCameraUserAttributePool::setDisableDualStreaming(const QnUuid& cameraId, bool value)
{
    return setValue(cameraId, &QnCameraUserAttributes::disableDualStreaming, value);
}

nx::vms::api::dewarping::MediaData QnCameraUserAttributePool::dewarpingParams(const QnUuid& cameraId)
{
    return getValue(cameraId, &QnCameraUserAttributes::dewarpingParams);
}

bool QnCameraUserAttributePool::setDewarpingParams(const QnUuid& cameraId, const nx::vms::api::dewarping::MediaData& value)
{
    return setValue(cameraId, &QnCameraUserAttributes::dewarpingParams, value);
}

QList<QnMotionRegion> QnCameraUserAttributePool::motionRegions(const QnUuid& cameraId)
{
    return getValue(cameraId, &QnCameraUserAttributes::motionRegions);
}

bool QnCameraUserAttributePool::setMotionRegions(const QnUuid& cameraId, const QList<QnMotionRegion>& value)
{
    return setValue(cameraId, &QnCameraUserAttributes::motionRegions, value);
}

nx::vms::api::MotionType QnCameraUserAttributePool::motionType(const QnUuid& cameraId)
{
    return getValue(cameraId, &QnCameraUserAttributes::motionType);
}

bool QnCameraUserAttributePool::setMotionType(const QnUuid& cameraId,
    nx::vms::api::MotionType value)
{
    return setValue(cameraId, &QnCameraUserAttributes::motionType, value);
}

QString QnCameraUserAttributePool::name(const QnUuid& cameraId)
{
    return getValue(cameraId, &QnCameraUserAttributes::name);
}

bool QnCameraUserAttributePool::setName(const QnUuid& cameraId, const QString& value)
{
    return setValue(cameraId, &QnCameraUserAttributes::name, value);
}

QString QnCameraUserAttributePool::groupName(const QnUuid& cameraId)
{
    return getValue(cameraId, &QnCameraUserAttributes::groupName);
}

bool QnCameraUserAttributePool::setGroupName(const QnUuid& cameraId, const QString& value)
{
    return setValue(cameraId, &QnCameraUserAttributes::groupName, value);
}

QString QnCameraUserAttributePool::logicalId(const QnUuid& cameraId)
{
    return getValue(cameraId, &QnCameraUserAttributes::logicalId);
}

bool QnCameraUserAttributePool::setLogicalId(const QnUuid& cameraId, const QString& value)
{
    return setValue(cameraId, &QnCameraUserAttributes::logicalId, value);
}

std::chrono::seconds QnCameraUserAttributePool::minPeriod(const QnUuid& cameraId)
{
    return getValue(cameraId, &QnCameraUserAttributes::minPeriodS);
}

bool QnCameraUserAttributePool::setMinPeriod(const QnUuid& cameraId, std::chrono::seconds value)
{
    return setValue(cameraId, &QnCameraUserAttributes::minPeriodS, value);
}

std::chrono::seconds QnCameraUserAttributePool::maxPeriod(const QnUuid& cameraId)
{
    return getValue(cameraId, &QnCameraUserAttributes::maxPeriodS);
}

bool QnCameraUserAttributePool::setMaxPeriod(const QnUuid& cameraId, std::chrono::seconds value)
{
    return setValue(cameraId, &QnCameraUserAttributes::maxPeriodS, value);
}

int QnCameraUserAttributePool::recordBeforeMotionSec(const QnUuid& cameraId)
{
    return getValue(cameraId, &QnCameraUserAttributes::recordBeforeMotionSec);
}

bool QnCameraUserAttributePool::setRecordBeforeMotionSec(const QnUuid& cameraId, int value)
{
    return setValue(cameraId, &QnCameraUserAttributes::recordBeforeMotionSec, value);
}

int QnCameraUserAttributePool::recordAfterMotionSec(const QnUuid& cameraId)
{
    return getValue(cameraId, &QnCameraUserAttributes::recordAfterMotionSec);
}

bool QnCameraUserAttributePool::setRecordAfterMotionSec(const QnUuid& cameraId, int value)
{
    return setValue(cameraId, &QnCameraUserAttributes::recordAfterMotionSec, value);
}

QnScheduleTaskList QnCameraUserAttributePool::scheduleTasks(const QnUuid& cameraId)
{
    return getValue(cameraId, &QnCameraUserAttributes::scheduleTasks);
}

bool QnCameraUserAttributePool::setScheduleTasks(const QnUuid& cameraId, QnScheduleTaskList value)
{
    return setValue(cameraId, &QnCameraUserAttributes::scheduleTasks, value);
}

nx::vms::api::CameraBackupQuality QnCameraUserAttributePool::backupQuality(const QnUuid& cameraId)
{
    return getValue(cameraId, &QnCameraUserAttributes::backupQuality);
}

bool QnCameraUserAttributePool::setBackupQuality(const QnUuid& cameraId, nx::vms::api::CameraBackupQuality value)
{
    return setValue(cameraId, &QnCameraUserAttributes::backupQuality, value);
}

nx::vms::api::BackupContentTypes QnCameraUserAttributePool::backupContentType(const QnUuid& cameraId)
{
    return getValue(cameraId, &QnCameraUserAttributes::backupContentType);
}

bool QnCameraUserAttributePool::setBackupContentType(const QnUuid& cameraId, nx::vms::api::BackupContentTypes value)
{
    return setValue(cameraId, &QnCameraUserAttributes::backupContentType, value);
}

nx::vms::api::BackupPolicy QnCameraUserAttributePool::backupPolicy(const QnUuid& cameraId)
{
    return getValue(cameraId, &QnCameraUserAttributes::backupPolicy);
}

bool QnCameraUserAttributePool::setBackupPolicy(const QnUuid& cameraId, nx::vms::api::BackupPolicy value)
{
    return setValue(cameraId, &QnCameraUserAttributes::backupPolicy, value);
}

void QnCameraUserAttributePool::assign(
    const QnUuid& cameraId,
    const QnCameraUserAttributes& sourceAttributes,
    QSet<QByteArray>* const modifiedFields)
{
    NX_WRITE_LOCKER locker{&m_mutex};
    m_elements[cameraId].assign(sourceAttributes, modifiedFields);
}

void QnCameraUserAttributePool::update(const QnUuid& cameraId, const QnCameraUserAttributes& value)
{
    NX_WRITE_LOCKER locker{&m_mutex};
    m_elements[cameraId] = value;
}

QnCameraUserAttributesPtr QnCameraUserAttributePool::getCopy(const QnUuid& cameraId)
{
    QnCameraUserAttributesPtr result(new QnCameraUserAttributes);
    result->cameraId = cameraId;

    NX_READ_LOCKER locker{&m_mutex};
    if (const auto itr = m_elements.find(cameraId); itr != m_elements.cend())
        *result = itr->second;

    return result;
}

QnCameraUserAttributesList QnCameraUserAttributePool::getAttributesList(const QList<QnUuid>& idList)
{
    NX_READ_LOCKER locker{&m_mutex};

    QnCameraUserAttributesList attributesList;
    attributesList.reserve(idList.size());
    for (const QnUuid id: idList)
    {
        QnCameraUserAttributesPtr attributes(new QnCameraUserAttributes);
        attributes->cameraId = id;
        if (const auto itr = m_elements.find(id); itr != m_elements.cend())
            *attributes = itr->second;

        attributesList.push_back(attributes);
    }
    return attributesList;
}

void QnCameraUserAttributePool::clear()
{
    NX_WRITE_LOCKER locker{&m_mutex};
    m_elements.clear();
}
