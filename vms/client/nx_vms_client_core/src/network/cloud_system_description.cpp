// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_system_description.h"

QnCloudSystemDescription::PointerType QnCloudSystemDescription::create(
    const QString& systemId,
    const QnUuid& localSystemId,
    const QString& systemName,
    const QString& ownerEmail,
    const QString& ownerFullName,
    bool online)
{
    return PointerType(new QnCloudSystemDescription(
        systemId, localSystemId, systemName, ownerEmail, ownerFullName, online));
}

QnCloudSystemDescription::QnCloudSystemDescription(
    const QString& systemId,
    const QnUuid& localSystemId,
    const QString& systemName,
    const QString& ownerEmail,
    const QString& ownerFullName,
    bool online)
    :
    base_type(systemId, localSystemId, systemName),
    m_ownerEmail(ownerEmail),
    m_ownerFullName(ownerFullName),
    m_online(online)
{
}

void QnCloudSystemDescription::setOnline(bool online)
{
    if (m_online == online)
        return;

    m_online = online;
    emit onlineStateChanged();
}

bool QnCloudSystemDescription::isCloudSystem() const
{
    return true;
}

bool QnCloudSystemDescription::isOnline() const
{
    return m_online;
}

bool QnCloudSystemDescription::isNewSystem() const
{
    return false;
}

QString QnCloudSystemDescription::ownerAccountEmail() const
{
    return m_ownerEmail;
}

QString QnCloudSystemDescription::ownerFullName() const
{
    return m_ownerFullName;
}
