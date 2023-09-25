// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QFile>
#include <QtGui/QPainter>
#include <QtSvg/QSvgRenderer>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/color_substitutions.h>
#include <nx/vms/client/core/skin/svg_customization.h>
#include <nx/vms/utils/external_resources.h>

#include "image_test_fixture.h"

namespace nx::vms::client::core {
namespace test {

class SvgCustomizationTest: public ImageTestFixture
{
public:
    SvgCustomizationTest():
        kTestFilePath(":/svg/test.svg"),
        sourceData(
            [path = kTestFilePath]() -> QByteArray
            {
                QFile source(path);
                NX_ASSERT(source.open(QIODevice::ReadOnly), "\"%1\" is not found", path);
                return source.readAll();
            }())
    {
    }

    void render(const QByteArray& svgData)
    {
        QSvgRenderer renderer;
        ASSERT_TRUE(renderer.load(svgData));

        image = QImage(20, 20, QImage::Format_ARGB32);
        image.fill(Qt::transparent);

        QPainter p(&image);
        renderer.render(&p);
    }

public:
    const QString kTestFilePath;
    const QByteArray sourceData;
};

TEST_F(SvgCustomizationTest, checkSourceFile)
{
    render(sourceData);
    EXPECT_TOP_LEFT("red");
    EXPECT_TOP_RIGHT("green");
    EXPECT_BOTTOM_LEFT("blue");
    EXPECT_BOTTOM_RIGHT("yellow");
}

TEST_F(SvgCustomizationTest, customizeSvgColorClasses)
{
    const auto customized = customizeSvgColorClasses(sourceData, {
        {"primary", Qt::cyan},
        {"secondary", Qt::magenta},
        {"tertiary", Qt::black}},
        /*debug name*/ kTestFilePath);

    render(customized);
    EXPECT_TOP_LEFT("cyan");
    EXPECT_TOP_RIGHT("magenta");
    EXPECT_BOTTOM_LEFT("black");
    EXPECT_BOTTOM_RIGHT("yellow");
}

TEST_F(SvgCustomizationTest, classCustomizationIsMissing)
{
    QSet<QString> classesMissingCustomization;

    customizeSvgColorClasses(sourceData, {{"primary", Qt::cyan}}, /*debug name*/ kTestFilePath,
        & classesMissingCustomization);

    EXPECT_EQ(classesMissingCustomization, QSet<QString>({"secondary", "tertiary"}));
}

TEST_F(SvgCustomizationTest, substituteSvgColors)
{
    const auto customized = substituteSvgColors(sourceData, {
        {Qt::red, Qt::cyan},
        {Qt::yellow, Qt::magenta}});

    render(customized);
    EXPECT_TOP_LEFT("cyan");
    EXPECT_TOP_RIGHT("green");
    EXPECT_BOTTOM_LEFT("blue");
    EXPECT_BOTTOM_RIGHT("magenta");
}

} // namespace test
} // namespace nx::vms::client::core
