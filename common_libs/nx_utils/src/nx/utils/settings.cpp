#include "settings.h"

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

void QnSettings::parseArgs(int argc, const char* argv[])
{
    m_args.parse(argc, argv);
    initializeSystemSettings();
}

bool QnSettings::contains(const QString& key) const
{
    return static_cast<bool>(m_args.get(key))
        || m_systemSettings->contains(key);
}

QVariant QnSettings::value(
    const QString& key,
    const QVariant& defaultValue) const
{
    if (const auto value = m_args.get(key))
        return QVariant(*value);

    return m_systemSettings->value(key, defaultValue);
}

void QnSettings::initializeSystemSettings()
{
    if (const auto config = m_args.get(lit("conf-file")))
    {
        m_systemSettings.reset(new QSettings(*config, QSettings::IniFormat));
    }
    else
    {
        #ifdef _WIN32
            m_systemSettings.reset(new QSettings(m_scope, m_organizationName, m_applicationName));
        #else
            m_systemSettings.reset(new QSettings(lit("/opt/%1/%2/etc/%2.conf")
                .arg(m_organizationName).arg(m_moduleName), QSettings::IniFormat));
        #endif
    }
}
