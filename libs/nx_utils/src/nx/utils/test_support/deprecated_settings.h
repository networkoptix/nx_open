// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/deprecated_settings.h>
#include "program_arguments.h"

namespace nx::utils::test {

/**
 * TestSettingsReader can be used by tests that need a SettingsReader to test loading functionality.
 * It parses the settings automatically when any of the SettingsReader::* functions are called.
 */
class NX_UTILS_API TestSettingsReader: public SettingsReader
{
public:
    TestSettingsReader();

    virtual bool contains(const QString& key) const override;

    virtual bool containsGroup(const QString& groupName) const override;

    virtual QVariant value(
        const QString& key,
        const QVariant& defaultValue = QVariant()) const override;

    virtual std::multimap<QString, QString> allArgs() const override;

    void addArg(std::string_view arg);

    void addArg(std::string_view name, std::string_view value);

    void parse() const;

private:
    void parseIfNeeded() const;

private:
    nx::utils::test::ProgramArguments m_args;
    mutable QnSettings m_settings;
    mutable bool m_parsed = false;
};

} // namespace nx::utils::test
