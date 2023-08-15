// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <string_view>

#include <QtCore/QSettings>

#include "argument_parser.h"

class SettingsReader
{
public:
    virtual ~SettingsReader() = default;

    virtual bool contains(const QString& key) const = 0;
    virtual bool containsGroup(const QString& groupName) const = 0;

    virtual QVariant value(
        const QString& key,
        const QVariant& defaultValue = QVariant()) const = 0;

    virtual std::multimap<QString, QString> allArgs() const = 0;
};

/**
 * Able to take settings from QSettings class (win32 registry or linux ini file) or from
 * command line arguments.
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
class NX_UTILS_API QnSettings:
    public SettingsReader
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

    virtual bool contains(const QString& key) const override;
    virtual bool containsGroup(const QString& groupName) const override;

    virtual QVariant value(
        const QString& key,
        const QVariant& defaultValue = QVariant()) const override;

    virtual std::multimap<QString, QString> allArgs() const override;

    QVariant value(
        const std::string_view& key,
        const QVariant& defaultValue = QVariant()) const;

    QVariant value(
        const char* key,
        const QVariant& defaultValue = QVariant()) const;

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
class NX_UTILS_API QnSettingsGroupReader:
    public SettingsReader
{
public:
    QnSettingsGroupReader(
        const SettingsReader& settings,
        const QString& name);

    virtual bool contains(const QString& key) const override;
    virtual bool containsGroup(const QString& groupName) const override;

    virtual QVariant value(
        const QString& key,
        const QVariant& defaultValue = QVariant()) const override;

    virtual std::multimap<QString, QString> allArgs() const override;

private:
    const SettingsReader& m_settings;
    QString m_groupName;
};
