// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "deprecated_settings.h"

#include <QtCore/QDir>

#include <nx/utils/string.h>

#include "app_info.h"

QnSettings::QnSettings(
    const QString& organizationName,
    const QString& applicationName,
    const QString& moduleName,
    QSettings::Scope scope)
:
    m_organizationName(organizationName),
    m_applicationName(applicationName),
    m_moduleName(moduleName),
    m_scope(scope)
{
}

QnSettings::QnSettings(QSettings* existingSettings):
    m_scope(existingSettings->scope()),
    m_systemSettings(existingSettings)
{
}

QnSettings::QnSettings(nx::utils::ArgumentParser args):
    m_args(std::move(args))
{
}

void QnSettings::parseArgs(int argc, const char* argv[])
{
    m_args.parse(argc, argv);
    initializeSystemSettings();
}

bool QnSettings::contains(const QString& key) const
{
    if (static_cast<bool>(m_args.get(key)))
        return true;
    if (m_systemSettings && m_systemSettings->contains(key))
        return true;
    return false;
}

bool QnSettings::containsGroup(const QString& groupSrc) const
{
    QString group = groupSrc;

    if (!group.endsWith('/'))
        group += '/';

    const auto allSettings = allArgs();
    auto it = allSettings.lower_bound(group);
    return it != allSettings.end() && it->first.startsWith(group);
}

QVariant QnSettings::value(
    const QString& key,
    const QVariant& defaultValue) const
{
    if (const auto value = m_args.get(key))
        return QVariant(*value);

    if (m_systemSettings)
        return m_systemSettings->value(key, defaultValue);

    return defaultValue;
}

std::multimap<QString, QString> QnSettings::allArgs() const
{
    std::multimap<QString, QString> args = m_args.allArgs();

    if (m_systemSettings)
    {
        const auto keys = m_systemSettings->allKeys();
        for (const auto& key: keys)
            args.emplace(key, m_systemSettings->value(key).toString());
    }

    return args;
}

QVariant QnSettings::value(
    const std::string_view& key,
    const QVariant& defaultValue) const
{
    return value(QString::fromUtf8(key.data(), key.size()), defaultValue);
}

QVariant QnSettings::value(
    const char* key,
    const QVariant& defaultValue) const
{
    return value(QString::fromUtf8(key), defaultValue);
}

void QnSettings::initializeSystemSettings()
{
    if (const auto config = m_args.get("conf-file"))
    {
        m_ownSettings = std::make_unique<QSettings>(*config, QSettings::IniFormat);
    }
    else if (m_args.contains("no-conf-file"))
    {
        // Not loading any config file. Application options are read from argv only.
    }
    else
    {
        #ifdef _WIN32
            m_ownSettings = std::make_unique<QSettings>(
                m_scope, m_organizationName, m_applicationName);
        #else
            m_ownSettings = std::make_unique<QSettings>(QString("/opt/%1/%2/etc/%2.conf")
                .arg(m_organizationName).arg(m_moduleName), QSettings::IniFormat);
        #endif
    }

    m_systemSettings = m_ownSettings.get();
}

//-------------------------------------------------------------------------------------------------

QnSettingsGroupReader::QnSettingsGroupReader(
    const SettingsReader& settings,
    const QString& name)
    :
    m_settings(settings),
    m_groupName(name)
{
}

bool QnSettingsGroupReader::contains(const QString& keyName) const
{
    QString key = m_groupName.isEmpty() ? keyName : m_groupName + "/" + keyName;
    return m_settings.contains(key);
}

bool QnSettingsGroupReader::containsGroup(const QString& groupName) const
{
    QString key = m_groupName.isEmpty() ? groupName : m_groupName + "/" + groupName;
    return m_settings.containsGroup(key);
}

QVariant QnSettingsGroupReader::value(
    const QString& keyName,
    const QVariant& defaultValue) const
{
    QString key = m_groupName.isEmpty() ? keyName : m_groupName + "/" + keyName;
    return m_settings.value(key, defaultValue);
}

std::multimap<QString, QString> QnSettingsGroupReader::allArgs() const
{
    const QString prefix = m_groupName.isEmpty() ? QString() : m_groupName + "/";
    const auto args = m_settings.allArgs();

    std::multimap<QString, QString> result;
    for (auto it = args.lower_bound(prefix); it != args.end(); ++it)
    {
        if (!it->first.startsWith(prefix))
            break;

        result.emplace(it->first.mid(prefix.size()), it->second);
    }

    return result;
}
