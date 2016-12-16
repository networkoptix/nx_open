#pragma once

#include <map>

#include <QtCore/QSettings>

#include "argument_parser.h"
#include "log/log.h"
#include "string.h"

/**
 * Able to take settings from QSettings class (win32 registry or linux ini file) or from
 *  command line arguments.
 *
 * Value defined as command line argument has preference over registry.
 *
 * example.reg
 *      [pathToModule/section]
 *      "item" = "value"
 *
 * example.ini
 *      [section]
 *      item = value
 *
 * arguments: --section/item=value
 */
class NX_UTILS_API QnSettings
{
public:
    QnSettings(
        const QString& organizationName,
        const QString& applicationName,
        const QString& moduleName,
        QSettings::Scope scope = QSettings::SystemScope);

    void parseArgs(int argc, const char* argv[]);
    bool contains(const QString& key) const;
    QVariant value(
        const QString& key,
        const QVariant& defaultValue = QVariant()) const;

    const QString getApplicationName() const { return m_applicationName; }
    const QString getModuleName() const { return m_moduleName; }

private:
    const QString m_applicationName;
    const QString m_moduleName;

    QSettings m_systemSettings;
    nx::utils::ArgumentParser m_args;
};
