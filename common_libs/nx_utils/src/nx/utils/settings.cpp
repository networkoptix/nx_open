#include "settings.h"

#include <QtCore/QDir>

#include "app_info.h"

QnSettings::QnSettings(
    const QString& organizationName,
    const QString& applicationName,
    const QString& moduleName,
    QSettings::Scope scope)
    :
    m_applicationName(applicationName),
    m_moduleName(moduleName),
    #ifdef _WIN32
        m_systemSettings(scope, organizationName, applicationName)
    #else
        m_systemSettings(lm("/opt/%1/%2/etc/%2.conf")
            .arg(organizationName).arg(moduleName), QSettings::IniFormat)
    #endif
{
    (void) scope; //< Unused on certain platforms.
}

void QnSettings::parseArgs(int argc, const char* argv[])
{
    m_args.parse(argc, argv);
}

bool QnSettings::contains(const QString& key) const
{
    return static_cast<bool>(m_args.get(key))
        || m_systemSettings.contains(key);
}

QVariant QnSettings::value(
    const QString& key,
    const QVariant& defaultValue) const
{
    if (const auto value = m_args.get(key))
        return QVariant(*value);

    return m_systemSettings.value(key, defaultValue);
}
