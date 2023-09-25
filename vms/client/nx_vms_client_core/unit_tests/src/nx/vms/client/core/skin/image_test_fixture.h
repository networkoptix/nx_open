// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtGui/QImage>

namespace nx::vms::client::core {
namespace test {

#define EXPECT_COLOR(x, y, color) \
    EXPECT_EQ(image.pixelColor(x, y), QColor(color))

#define EXPECT_TOP_LEFT(color) EXPECT_COLOR(0, 0, color)
#define EXPECT_TOP_RIGHT(color) EXPECT_COLOR(image.width() - 1, 0, color)
#define EXPECT_BOTTOM_LEFT(color) EXPECT_COLOR(0, image.height() - 1, color)
#define EXPECT_BOTTOM_RIGHT(color) EXPECT_COLOR(image.width() - 1, image.height() - 1, color)

class ImageTestFixture: public ::testing::Test
{
public:
    virtual void TearDown() override
    {
        image = {};
    }

public:
    QImage image;
};

} // namespace test
} // namespace nx::vms::client::core
