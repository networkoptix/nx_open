// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "deprecated_settings.h"

namespace nx::utils::test {

TestSettingsReader::TestSettingsReader():
    m_settings(nx::utils::ArgumentParser{})
{
    // Do not attempt to get settings from a file, only from the args given through addArg().
    m_args.addArg("no-conf-file");
}

bool TestSettingsReader::contains(const QString& key) const
{
    parseIfNeeded();
    return m_settings.contains(key);
}

bool TestSettingsReader::containsGroup(const QString& groupName) const
{
    parseIfNeeded();
    return m_settings.containsGroup(groupName);
}

QVariant TestSettingsReader::value(
    const QString& key,
    const QVariant& defaultValue) const
{
    parseIfNeeded();
    return m_settings.value(key, defaultValue);
}

std::multimap<QString, QString> TestSettingsReader::allArgs() const
{
    parseIfNeeded();
    return m_settings.allArgs();
}

void TestSettingsReader::addArg(std::string_view arg)
{
    m_parsed = false;
    m_args.addArg(arg.data());
}

void TestSettingsReader::addArg(std::string_view name, std::string_view value)
{
    m_parsed = false;
    m_args.addArg(name.data(), value.data());
}

void TestSettingsReader::parse() const
{
    m_parsed = true;
    m_settings.parseArgs(m_args.argc(), m_args.constArgv());
}

void TestSettingsReader::parseIfNeeded() const
{
    if (!m_parsed)
        parse();
}

} // namespace nx::utils::test
