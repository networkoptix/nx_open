// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <string_view>

#include <QtCore/QSettings>

#include "argument_parser.h"

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

    /**
     * Initialize using existing QSettings object.
     * Caller is responsible for existingSettings life time.
     */
    QnSettings(QSettings* existingSettings);
    QnSettings(nx::utils::ArgumentParser args);

    void parseArgs(int argc, const char* argv[]);

    bool contains(const QString& key) const;
    bool containsGroup(QString groupName) const;

    QVariant value(
        const QString& key,
        const QVariant& defaultValue = QVariant()) const;

    QVariant value(
        const std::string_view& key,
        const QVariant& defaultValue = QVariant()) const;

    QVariant value(
        const char* key,
        const QVariant& defaultValue = QVariant()) const;

    std::multimap<QString, QString> allArgs() const;

    const QString getApplicationName() const { return m_applicationName; }
    const QString getModuleName() const { return m_moduleName; }

private:
    void initializeSystemSettings();

    const QString m_organizationName;
    const QString m_applicationName;
    const QString m_moduleName;
    const QSettings::Scope m_scope = QSettings::Scope::UserScope;

    std::unique_ptr<QSettings> m_ownSettings;
    QSettings* m_systemSettings = nullptr;
    nx::utils::ArgumentParser m_args;
};

//-------------------------------------------------------------------------------------------------

/**
 * Useful for reading from a specific group.
 */
class NX_UTILS_API QnSettingsGroupReader
{
public:
    QnSettingsGroupReader(
        const QnSettings& settings,
        const std::string& name);

    bool contains(const std::string& key) const;

    QVariant value(
        const std::string_view& key,
        const QVariant& defaultValue = QVariant()) const;

private:
    const QnSettings& m_settings;
    std::string m_groupName;
};

