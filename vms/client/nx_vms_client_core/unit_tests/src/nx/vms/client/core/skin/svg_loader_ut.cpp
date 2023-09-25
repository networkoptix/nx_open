// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <memory>

#include <QtGui/QPixmap>
#include <QtGui/QPixmapCache>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/svg_loader.h>
#include <nx/vms/utils/external_resources.h>

#include "image_test_fixture.h"

namespace nx::vms::client::core {
namespace test {

class SvgLoaderTest: public ImageTestFixture
{
public:
    SvgLoaderTest()
    {
        nx::vms::utils::registerExternalResource("client_core_external.dat");
    }

    virtual void SetUp() override
    {
        colorThemePtr.reset(new ColorTheme());
    }

    virtual void TearDown() override
    {
        colorThemePtr.reset();
        QPixmapCache::clear();
        ImageTestFixture::TearDown();
    }

public:
    const QString kTestFilePath{":/svg/test.svg"};
    const QSize kTestImageSize{20, 20};

    const QMap<QString, QColor> kDefaultCustomization{
        {"primary", Qt::cyan},
        {"secondary", Qt::magenta},
        {"tertiary", Qt::black}};

    std::unique_ptr<ColorTheme> colorThemePtr;
};

TEST_F(SvgLoaderTest, defaultSize)
{
    image = loadSvgImage(kTestFilePath, kDefaultCustomization).toImage();
    EXPECT_EQ(image.size(), kTestImageSize);
}

TEST_F(SvgLoaderTest, defaultSizeDPIScaled)
{
    const qreal testDPR = 1.5;
    image = loadSvgImage(kTestFilePath, kDefaultCustomization, {}, testDPR).toImage();
    EXPECT_EQ(image.size(), kTestImageSize * testDPR);
    EXPECT_EQ(image.devicePixelRatio(), testDPR);
}

TEST_F(SvgLoaderTest, explicitSize)
{
    const QSize testSize{30, 30};
    image = loadSvgImage(kTestFilePath, kDefaultCustomization, testSize).toImage();
    EXPECT_EQ(image.size(), testSize);
}

TEST_F(SvgLoaderTest, explicitSizeNotDPIScaled)
{
    const qreal testDPR = 1.5;
    const QSize testSize{10, 10};
    image = loadSvgImage(kTestFilePath, kDefaultCustomization, testSize, testDPR).toImage();
    EXPECT_EQ(image.size(), testSize);
    EXPECT_EQ(image.devicePixelRatio(), testDPR);
}

TEST_F(SvgLoaderTest, customizationMapWithColors)
{
    image = loadSvgImage(kTestFilePath, kDefaultCustomization).toImage();
    EXPECT_TOP_LEFT("cyan");
    EXPECT_TOP_RIGHT("magenta");
    EXPECT_BOTTOM_LEFT("black");
    EXPECT_BOTTOM_RIGHT("yellow");
}

TEST_F(SvgLoaderTest, customizationMapWithNames)
{
    const QMap<QString, QString> customization{
        {"primary", "cyan"},
        {"secondary", "#101010"},
        {"tertiary", "dark5"}};

    image = loadSvgImage(kTestFilePath, customization).toImage();
    EXPECT_TOP_LEFT("cyan");
    EXPECT_TOP_RIGHT("#101010");
    EXPECT_BOTTOM_LEFT(colorTheme()->color("dark5"));
    EXPECT_BOTTOM_RIGHT("yellow");
}

TEST_F(SvgLoaderTest, colorThemeSubstitutions)
{
    // This test ColorTheme has one `brand_core` color,
    // default is yellow #FFFF00, customized is #808080.
    colorThemePtr.reset(); //< Must destroy the singleton instance before creating a new one.
    colorThemePtr.reset(new ColorTheme(
        ":/unit_tests/svg_loader_ut/basic_colors.json",
        ":/unit_tests/svg_loader_ut/skin_colors.json"));

    ASSERT_EQ(colorTheme()->color("brand_core"), QColor("#808080"));

    const QMap<QString, QString> customization{
        {"primary", "white"},
        {"secondary", "white"},
        {"tertiary", "white"}};

    image = loadSvgImage(kTestFilePath, customization).toImage();
    EXPECT_TOP_LEFT("white");
    EXPECT_TOP_RIGHT("white");
    EXPECT_BOTTOM_LEFT("white");
    EXPECT_BOTTOM_RIGHT("#808080");
}

} // namespace test
} // namespace nx::core::access
