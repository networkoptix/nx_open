// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/deprecated_settings.h>

namespace test {

TEST(QnSettingsGroupReader, allArgs)
{
    QnSettings settings(nx::ArgumentParser(
        {"-listener/port", "22", "-listener/backlog", "1024", "-a/b", "c", "-x/y", "z"}));

    const std::multimap<QString, QString> expected{{"backlog", "1024"} , {"port", "22"}};
    ASSERT_EQ(expected, ::QnSettingsGroupReader(settings, "listener").allArgs());
}

} // namespace test
