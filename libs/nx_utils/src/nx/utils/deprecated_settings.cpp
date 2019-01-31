#include "deprecated_settings.h"

#include <QtCore/QDir>

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

QVariant QnSettings::value(
    const QString& key,
    const QVariant& defaultValue) const
{
    if (const auto value = m_args.get(key))
        return QVariant(*value);

    if (m_systemSettings)
        return m_systemSettings->value(key, defaultValue);

    return QVariant();
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

void QnSettings::initializeSystemSettings()
{
    if (const auto config = m_args.get("conf-file"))
    {
        m_ownSettings.reset(new QSettings(*config, QSettings::IniFormat));
    }
    else
    {
        #ifdef _WIN32
            m_ownSettings.reset(new QSettings(m_scope, m_organizationName, m_applicationName));
        #else
            m_ownSettings.reset(new QSettings(QString("/opt/%1/%2/etc/%2.conf")
                .arg(m_organizationName).arg(m_moduleName), QSettings::IniFormat));
        #endif
    }

    m_systemSettings = m_ownSettings.get();
}
