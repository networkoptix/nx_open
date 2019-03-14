#include "cloud_system_description.h"

QnCloudSystemDescription::PointerType QnCloudSystemDescription::create(
    const QString& systemId,
    const QnUuid& localSystemId,
    const QString& systemName,
    const QString& ownerEmail,
    const QString& ownerFullName,
    bool running)
{
    return PointerType(new QnCloudSystemDescription(
        systemId, localSystemId, systemName, ownerEmail, ownerFullName, running));
}

QnCloudSystemDescription::QnCloudSystemDescription(
    const QString& systemId,
    const QnUuid& localSystemId,
    const QString& systemName,
    const QString& ownerEmail,
    const QString& ownerFullName,
    bool running)
    :
    base_type(systemId, localSystemId, systemName),
    m_ownerEmail(ownerEmail),
    m_ownerFullName(ownerFullName),
    m_running(running)
{
}

void QnCloudSystemDescription::setRunning(bool running)
{
    if (m_running == running)
        return;

    m_running = running;
    emit runningStateChanged();
}

bool QnCloudSystemDescription::isCloudSystem() const
{
    return true;
}

bool QnCloudSystemDescription::isRunning() const
{
    return m_running;
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
