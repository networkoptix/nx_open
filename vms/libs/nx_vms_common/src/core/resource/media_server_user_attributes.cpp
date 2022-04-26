// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_server_user_attributes.h"

#include <nx/fusion/model_functions.h>

QnMediaServerUserAttributes::QnMediaServerUserAttributes(
    const nx::vms::api::MediaServerUserAttributesData& data)
    :
    m_data(data)
{
}

void QnMediaServerUserAttributes::assign(
    const QnMediaServerUserAttributes& right, QSet<QByteArray>* const modifiedFields)
{
    if (m_data.allowAutoRedundancy != right.m_data.allowAutoRedundancy)
        modifiedFields->insert("redundancyChanged");

    if (m_data.serverName != right.m_data.serverName)
        modifiedFields->insert("nameChanged");

    if (m_data.backupBitrateBytesPerSecond != right.m_data.backupBitrateBytesPerSecond)
        modifiedFields->insert("backupBitrateLimitsChanged");

    *this = right;
}

bool QnMediaServerUserAttributes::isRedundancyEnabled() const
{
    return m_data.allowAutoRedundancy;
}

void QnMediaServerUserAttributes::setIsRedundancyEnabled(bool value)
{
    m_data.allowAutoRedundancy = value;
}

QnUuid QnMediaServerUserAttributes::serverId() const
{
    return m_data.serverId;
}

void QnMediaServerUserAttributes::setServerId(const QnUuid& serverId)
{
    m_data.serverId = serverId;
}

QString QnMediaServerUserAttributes::name() const
{
    return m_data.serverName;
}

void QnMediaServerUserAttributes::setName(const QString& name)
{
    m_data.serverName = name;
}

int QnMediaServerUserAttributes::maxCameras() const
{
    return m_data.maxCameras;
}

void QnMediaServerUserAttributes::setMaxCameras(int value)
{
    m_data.maxCameras = value;
}

int QnMediaServerUserAttributes::locationId() const
{
    return m_data.locationId;
}

void QnMediaServerUserAttributes::setLocationId(int value)
{
    m_data.locationId = value;
}

nx::vms::api::MediaServerUserAttributesData QnMediaServerUserAttributes::data() const
{
    return m_data;
}

void QnMediaServerUserAttributes::setData(const nx::vms::api::MediaServerUserAttributesData& data)
{
    m_data = data;
}

void QnMediaServerUserAttributes::setBackupBitrateBytesPerSecond(
    const nx::vms::api::BackupBitrateBytesPerSecond& backupBitrateBytesPerSecond)
{
    m_data.backupBitrateBytesPerSecond = backupBitrateBytesPerSecond;
}

nx::vms::api::BackupBitrateBytesPerSecond
    QnMediaServerUserAttributes::backupBitrateBytesPerSecond() const
{
    return m_data.backupBitrateBytesPerSecond;
}


QnMediaServerUserAttributesPool::QnMediaServerUserAttributesPool(QObject *parent):
    QObject(parent)
{
    setElementInitializer(
        [](const QnUuid& serverId, QnMediaServerUserAttributesPtr& userAttributes)
        {
            userAttributes = QnMediaServerUserAttributesPtr(new QnMediaServerUserAttributes());
            userAttributes->setServerId(serverId);
        });
}

QnMediaServerUserAttributesList QnMediaServerUserAttributesPool::getAttributesList(
    const QList<QnUuid>& idList)
{
    QnMediaServerUserAttributesList valList;
    valList.reserve(idList.size());
    for (const QnUuid id: idList)
        valList.push_back(*ScopedLock(this, id));
    return valList;
}
