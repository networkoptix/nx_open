// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtGui/QColor>

#include <nx/reflect/to_string.h>
#include <nx/reflect/from_string.h>
#include <nx/vms/common/serialization/qt_gui_types.h>

TEST(QtGuiTypesSerialization, QColor)
{
    EXPECT_EQ("#123456", nx::reflect::toString(QColor("#123456")));
    EXPECT_EQ(QColor("#123456"), nx::reflect::fromString<QColor>("#123456", {}));

    EXPECT_EQ(QColor(Qt::red), nx::reflect::fromString<QColor>("red", {}));

    EXPECT_EQ("#12345678", nx::reflect::toString(QColor("#12345678")));
    EXPECT_EQ(QColor("#12345678"), nx::reflect::fromString<QColor>("#12345678", {}));
}
