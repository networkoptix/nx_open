// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/common/utils/object_painter_helper.h>

namespace nx::vms::common {
namespace test {

QMarginsF calculateSpace(QRectF objectRect, QRectF boundingRect)
{
    return QMarginsF(
        objectRect.left() - boundingRect.left(),
        objectRect.top() - boundingRect.top(),
        boundingRect.right() - objectRect.right(),
        boundingRect.bottom() - objectRect.bottom());
}

TEST(ObjectPainterHelper, hasSpace)
{
    QMargins margins;
    ASSERT_FALSE(ObjectPainterHelper::hasHorizontalSpace(margins));
    ASSERT_FALSE(ObjectPainterHelper::hasVerticalSpace(margins));
    margins = {10, 10, 10, 10};
    ASSERT_FALSE(ObjectPainterHelper::hasHorizontalSpace(margins));
    ASSERT_FALSE(ObjectPainterHelper::hasVerticalSpace(margins));

    margins = {20, 10, 20, 10};
    ASSERT_TRUE(ObjectPainterHelper::hasHorizontalSpace(margins));
    ASSERT_FALSE(ObjectPainterHelper::hasVerticalSpace(margins));
    margins = {10, 10, 20, 10};
    ASSERT_FALSE(ObjectPainterHelper::hasHorizontalSpace(margins));
    ASSERT_FALSE(ObjectPainterHelper::hasVerticalSpace(margins));
    margins = {20, 10, 10, 10};
    ASSERT_FALSE(ObjectPainterHelper::hasHorizontalSpace(margins));
    ASSERT_FALSE(ObjectPainterHelper::hasVerticalSpace(margins));

    margins = {10, 20, 10, 20};
    ASSERT_FALSE(ObjectPainterHelper::hasHorizontalSpace(margins));
    ASSERT_TRUE(ObjectPainterHelper::hasVerticalSpace(margins));
    margins = {10, 10, 10, 20};
    ASSERT_FALSE(ObjectPainterHelper::hasHorizontalSpace(margins));
    ASSERT_FALSE(ObjectPainterHelper::hasVerticalSpace(margins));
    margins = {10, 20, 10, 10};
    ASSERT_FALSE(ObjectPainterHelper::hasHorizontalSpace(margins));
    ASSERT_FALSE(ObjectPainterHelper::hasVerticalSpace(margins));

    margins = {20, 20, 20, 20};
    ASSERT_TRUE(ObjectPainterHelper::hasHorizontalSpace(margins));
    ASSERT_TRUE(ObjectPainterHelper::hasVerticalSpace(margins));
    margins = {10, 20, 20, 20};
    ASSERT_FALSE(ObjectPainterHelper::hasHorizontalSpace(margins));
    ASSERT_TRUE(ObjectPainterHelper::hasVerticalSpace(margins));
    margins = {20, 10, 20, 20};
    ASSERT_TRUE(ObjectPainterHelper::hasHorizontalSpace(margins));
    ASSERT_FALSE(ObjectPainterHelper::hasVerticalSpace(margins));
    margins = {20, 20, 10, 20};
    ASSERT_FALSE(ObjectPainterHelper::hasHorizontalSpace(margins));
    ASSERT_TRUE(ObjectPainterHelper::hasVerticalSpace(margins));
    margins = {20, 20, 20, 10};
    ASSERT_TRUE(ObjectPainterHelper::hasHorizontalSpace(margins));
    ASSERT_FALSE(ObjectPainterHelper::hasVerticalSpace(margins));
}

TEST(ObjectPainterHelper, findBestSide)
{
    QSizeF labelSize(100, 100);
    QMarginsF space = {150, 80, 2000, 80};
    auto side = ObjectPainterHelper::findBestSide(labelSize, space);
    ASSERT_EQ(side.edge, Qt::Edge::LeftEdge);

    space = {70, 80, 150, 80};
    side = ObjectPainterHelper::findBestSide(labelSize, space);
    ASSERT_EQ(side.edge, Qt::Edge::RightEdge);

    space = {150, 200, 150, 80};
    side = ObjectPainterHelper::findBestSide(labelSize, space);
    ASSERT_EQ(side.edge, Qt::Edge::TopEdge);

    space = {200, 200, 200, 150};
    side = ObjectPainterHelper::findBestSide(labelSize, space);
    ASSERT_EQ(side.edge, Qt::Edge::BottomEdge);
}

struct LabelGeometryCase
{
    const char* name;
    QRectF boundingRect;
    QRectF objectRect;
    Qt::Edge expectedEdge;
};

class ObjectPainterHelper_CalculateLabelGeometry:
    public ::testing::TestWithParam<LabelGeometryCase>
{
};

TEST_P(ObjectPainterHelper_CalculateLabelGeometry, Works)
{
    const auto& p = GetParam();

    const QSizeF labelSize = {100, 100};
    const QMarginsF labelMargins = {10, 10, 10, 10};

    const QRectF labelGeometry = ObjectPainterHelper::calculateLabelGeometry(
        p.boundingRect, labelSize, labelMargins, p.objectRect);

    ASSERT_TRUE(labelGeometry.isValid());
    ASSERT_TRUE(labelGeometry.top() >= 24); // Header size.
    ASSERT_TRUE(p.boundingRect.contains(labelGeometry));

    const auto bestSide = ObjectPainterHelper::findBestSide(
        labelSize, calculateSpace(p.objectRect, p.boundingRect));

    ASSERT_EQ(bestSide.edge, p.expectedEdge);

    switch (p.expectedEdge)
    {
        case Qt::BottomEdge:
            EXPECT_EQ(labelGeometry.top(), p.objectRect.bottom());
            EXPECT_LE(labelGeometry.left(), p.objectRect.left());
            break;

        case Qt::TopEdge:
            EXPECT_EQ(labelGeometry.bottom(), p.objectRect.top());
            EXPECT_LE(labelGeometry.left(), p.objectRect.left());
            break;

        case Qt::LeftEdge:
            EXPECT_EQ(labelGeometry.right(), p.objectRect.left());
            EXPECT_LE(labelGeometry.top(), p.objectRect.top());
            break;

        case Qt::RightEdge:
            EXPECT_EQ(labelGeometry.left(), p.objectRect.right());
            EXPECT_LE(labelGeometry.top(), p.objectRect.top());
            break;
        default:
            break;
    }
}

INSTANTIATE_TEST_SUITE_P(
    ObjectPainterHelper,
    ObjectPainterHelper_CalculateLabelGeometry,
    ::testing::Values(
        LabelGeometryCase{
            "Bottom",
            QRectF{0, 0, 1000, 1000},
            QRectF{200, 200, 100, 100},
            Qt::BottomEdge},

        LabelGeometryCase{
            "Top",
            QRectF{0, 0, 1000, 1000},
            QRectF{200, 800, 100, 100},
            Qt::TopEdge},

        LabelGeometryCase{
            "Left",
            QRectF{0, 0, 1000, 200},
            QRectF{200, 50, 100, 100},
            Qt::LeftEdge},

        LabelGeometryCase{
            "Right",
            QRectF{0, 0, 1000, 200},
            QRectF{80, 50, 100, 100},
            Qt::RightEdge}),
    [](const ::testing::TestParamInfo<LabelGeometryCase>& info)
    {
        return info.param.name;
    });

TEST(ObjectPainterHelper, calculateTooltipColor)
{
    ASSERT_EQ(
        ObjectPainterHelper::calculateTooltipColor(QColor(0xFFFFFF)).toRgb().name(),
        QColor(10, 10, 10, 127).name());

    // "red":          "#C62828",
    ASSERT_EQ(
        ObjectPainterHelper::calculateTooltipColor(QColor(0xC62828)).toRgb().name(),
        QColor(17, 3, 3, 127).name());

    //"yellow":       "#FF8F00",
    ASSERT_EQ(
        ObjectPainterHelper::calculateTooltipColor(QColor(0xFF8F00)).toRgb().name(),
        QColor(20, 11, 0, 127).name());

    //"blue":         "#1565C0",
    ASSERT_EQ(
        ObjectPainterHelper::calculateTooltipColor(QColor(0x1565C0)).toRgb().name(),
        QColor(2, 10, 18, 127).name());
}

} // namespace test
} // namespace nx::vms::common
