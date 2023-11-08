// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_system_description.h"

#include <nx/utils/software_version.h>

QnCloudSystemDescriptionPtr QnCloudSystemDescription::create(
    const QString& systemId,
    const QnUuid& localSystemId,
    const QString& systemName,
    const QString& ownerEmail,
    const QString& ownerFullName,
    bool online,
    bool system2faEnabled,
    const QString& organizationId)
{
    return QnCloudSystemDescriptionPtr(new QnCloudSystemDescription(
        systemId, localSystemId, systemName, ownerEmail, ownerFullName, online, system2faEnabled, organizationId));
}

QnCloudSystemDescription::QnCloudSystemDescription(
    const QString& systemId,
    const QnUuid& localSystemId,
    const QString& systemName,
    const QString& ownerEmail,
    const QString& ownerFullName,
    bool online,
    bool system2faEnabled,
    const QString& organizationId)
    :
    base_type(systemId, localSystemId, systemName),
    m_ownerEmail(ownerEmail),
    m_ownerFullName(ownerFullName),
    m_online(online),
    m_system2faEnabled(system2faEnabled),
    m_organizationId(organizationId)
{
}

void QnCloudSystemDescription::setOnline(bool online)
{
    if (m_online == online)
        return;

    m_online = online;
    emit onlineStateChanged();
}

void QnCloudSystemDescription::set2faEnabled(bool system2faEnabled)
{
    if (m_system2faEnabled == system2faEnabled)
        return;

    m_system2faEnabled = system2faEnabled;
    emit system2faEnabledChanged();
}

void QnCloudSystemDescription::setOrganization(const QString& organizationId)
{
    if (m_organizationId == organizationId)
        return;

    m_organizationId = organizationId;
    emit organizationChanged();
}

void QnCloudSystemDescription::updateLastKnownVersion(const nx::utils::SoftwareVersion& version)
{
    if (version == m_lastKnownVersion)
        return;

    m_lastKnownVersion = version;
    emit oauthSupportedChanged();
}

bool QnCloudSystemDescription::isCloudSystem() const
{
    return true;
}

bool QnCloudSystemDescription::is2FaEnabled() const
{
    return m_system2faEnabled;
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

bool QnCloudSystemDescription::isOauthSupported() const
{
    static const auto kOauthStartSupportVersion = nx::utils::SoftwareVersion(5, 0);
    return m_lastKnownVersion.isNull() || m_lastKnownVersion >= kOauthStartSupportVersion;
}

nx::utils::SoftwareVersion QnCloudSystemDescription::version() const
{
    return std::max(m_lastKnownVersion, base_type::version());
}
