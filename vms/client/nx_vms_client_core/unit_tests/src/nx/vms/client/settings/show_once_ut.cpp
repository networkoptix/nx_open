// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QSettings>

#include <settings/show_once.h>

namespace nx::settings {

TEST(ShowOnceTest, resetWorksProperly)
{
    QCoreApplication::setOrganizationName("Test");
    QCoreApplication::setApplicationName("Test App");

    QSettings settings;

    const QString rootLevelKey{"rootLevelKey"};
    settings.setValue(rootLevelKey, 1);

    const QString showOnceId = "showOnceID";
    ShowOnce showOnce(showOnceId, ShowOnce::StorageFormat::Section);

    const QString flagName = "flag";
    showOnce.setFlag(flagName, true);

    ASSERT_EQ(showOnce.testFlag(flagName), true);
    ASSERT_EQ(settings.value(QString("%1/%2").arg(showOnceId, flagName)).toBool(), true);

    showOnce.reset();

    // Check if removed only items belongs to the ShowOnce instance.
    ASSERT_EQ(showOnce.testFlag(flagName), false);
    ASSERT_EQ(settings.value(QString("%1/%2").arg(showOnceId, flagName)).toBool(), false);
    ASSERT_TRUE(settings.contains(rootLevelKey));

    // Check if the ShowOnce instance continue works correctly after reset.
    showOnce.setFlag(flagName, true);
    ASSERT_EQ(showOnce.testFlag(flagName), true);
    ASSERT_EQ(settings.value(QString("%1/%2").arg(showOnceId, flagName)).toBool(), true);
}

} // namespace nx::settings
