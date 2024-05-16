// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/client/core/event_search/models/detail/preview_rect_calculator.h>

namespace nx::vms::client::core {
namespace test {

namespace {

static const QnAspectRatio kTargetAr(16, 9);
static const QRectF kUnitRect(0, 0, 1, 1);

} // namespace

TEST(previewRectCalculator, emptyRect)
{
    QRectF cropRect;
    QRectF croppedHighlightRect;
    const QnAspectRatio resourceAr(3, 4);
    const QRectF highlightRect = {};
    calculatePreviewRects(resourceAr, highlightRect, kTargetAr, cropRect, croppedHighlightRect);
    ASSERT_EQ(cropRect, QRectF());
    ASSERT_EQ(croppedHighlightRect, QRectF());
}

TEST(previewRectCalculator, unitRect)
{
    QRectF cropRect;
    QRectF croppedHighlightRect;
    const QnAspectRatio resourceAr(3, 4);
    const QRectF highlightRect = kUnitRect;
    calculatePreviewRects(resourceAr, highlightRect, kTargetAr, cropRect, croppedHighlightRect);
    ASSERT_EQ(cropRect, kUnitRect);
    ASSERT_EQ(croppedHighlightRect, kUnitRect);
}

TEST(previewRectCalculator, entireRect)
{
    const qreal width = 0.5;
    const QnAspectRatio resourceAr(3, 4);
    const QSizeF highlightSize(width, width * resourceAr.toFloat() / kTargetAr.toFloat());
    const QRectF highlightRect(QPointF(0.1, 0.2), highlightSize);

    QRectF cropRect;
    QRectF croppedHighlightRect;
    calculatePreviewRects(resourceAr, highlightRect, kTargetAr, cropRect, croppedHighlightRect);
    ASSERT_EQ(cropRect, highlightRect);
    ASSERT_EQ(croppedHighlightRect, kUnitRect);
}

TEST(previewRectCalculator, horizontalRect)
{
    QRectF cropRect;
    QRectF croppedHighlightRect;
    const auto resourceAr = kTargetAr; //< For simplicity.
    const QRectF highlightRect(QPointF(0.2, 0.4), QSizeF(0.6, 0.3));
    calculatePreviewRects(resourceAr, highlightRect, kTargetAr, cropRect, croppedHighlightRect);
    ASSERT_EQ(cropRect, QRectF(QPointF(0.2, 0.25), QSizeF(0.6, 0.6)));
    ASSERT_EQ(croppedHighlightRect, QRectF(QPointF(0.0, 0.25), QSizeF(1.0, 0.5)));
}

TEST(previewRectCalculator, topClippedHorizontalRect)
{
    QRectF cropRect;
    QRectF croppedHighlightRect;
    const auto resourceAr = kTargetAr; //< For simplicity.
    const QRectF highlightRect(QPointF(0.2, 0.1), QSizeF(0.6, 0.3));
    calculatePreviewRects(resourceAr, highlightRect, kTargetAr, cropRect, croppedHighlightRect);
    ASSERT_EQ(cropRect, QRectF(QPointF(0.2, 0.0), QSizeF(0.6, 0.6)));
    ASSERT_EQ(croppedHighlightRect, QRectF(QPointF(0.0, 1.0 / 6.0), QSizeF(1.0, 0.5)));
}

TEST(previewRectCalculator, bottomClippedHorizontalRect)
{
    QRectF cropRect;
    QRectF croppedHighlightRect;
    const auto resourceAr = kTargetAr; //< For simplicity.
    const QRectF highlightRect(QPointF(0.2, 0.6), QSizeF(0.6, 0.3));
    calculatePreviewRects(resourceAr, highlightRect, kTargetAr, cropRect, croppedHighlightRect);
    ASSERT_EQ(cropRect, QRectF(QPointF(0.2, 0.4), QSizeF(0.6, 0.6)));
    ASSERT_EQ(croppedHighlightRect, QRectF(QPointF(0.0, 2.0 / 6.0), QSizeF(1.0, 0.5)));
}

TEST(previewRectCalculator, topAndBottomClippedHorizontalRect)
{
    QRectF cropRect;
    QRectF croppedHighlightRect;
    const QnAspectRatio resourceAr(1, 1);
    const QnAspectRatio targetAr(1, 2);
    const QRectF highlightRect(QPointF(0.1, 0.2), QSizeF(0.7, 0.7));
    calculatePreviewRects(resourceAr, highlightRect, targetAr, cropRect, croppedHighlightRect);
    ASSERT_EQ(cropRect, QRectF(QPointF(0.1, 0.0), QSizeF(0.7, 1.0)));
    ASSERT_EQ(croppedHighlightRect, QRectF(QPointF(0.0, 0.2), QSizeF(1.0, 0.7)));
}

TEST(previewRectCalculator, verticalRect)
{
    QRectF cropRect;
    QRectF croppedHighlightRect;
    const auto resourceAr = kTargetAr; //< For simplicity.
    const QRectF highlightRect(QPointF(0.4, 0.2), QSizeF(0.3, 0.6));
    calculatePreviewRects(resourceAr, highlightRect, kTargetAr, cropRect, croppedHighlightRect);
    ASSERT_EQ(cropRect, QRectF(QPointF(0.25, 0.2), QSizeF(0.6, 0.6)));
    ASSERT_EQ(croppedHighlightRect, QRectF(QPointF(0.25, 0.0), QSizeF(0.5, 1.0)));
}

TEST(previewRectCalculator, leftClippedVerticalRect)
{
    QRectF cropRect;
    QRectF croppedHighlightRect;
    const auto resourceAr = kTargetAr; //< For simplicity.
    const QRectF highlightRect(QPointF(0.1, 0.2), QSizeF(0.3, 0.6));
    calculatePreviewRects(resourceAr, highlightRect, kTargetAr, cropRect, croppedHighlightRect);
    ASSERT_EQ(cropRect, QRectF(QPointF(0.0, 0.2), QSizeF(0.6, 0.6)));
    ASSERT_EQ(croppedHighlightRect, QRectF(QPointF(1.0 / 6.0, 0.0), QSizeF(0.5, 1.0)));
}

TEST(previewRectCalculator, rightClippedVerticalRect)
{
    QRectF cropRect;
    QRectF croppedHighlightRect;
    const auto resourceAr = kTargetAr; //< For simplicity.
    const QRectF highlightRect(QPointF(0.6, 0.2), QSizeF(0.3, 0.6));
    calculatePreviewRects(resourceAr, highlightRect, kTargetAr, cropRect, croppedHighlightRect);
    ASSERT_EQ(cropRect, QRectF(QPointF(0.4, 0.2), QSizeF(0.6, 0.6)));
    ASSERT_EQ(croppedHighlightRect, QRectF(QPointF(2.0 / 6.0, 0.0), QSizeF(0.5, 1.0)));
}

TEST(previewRectCalculator, leftAndRightClippedVerticalRect)
{
    QRectF cropRect;
    QRectF croppedHighlightRect;
    const QnAspectRatio resourceAr(1, 1);
    const QnAspectRatio targetAr(2, 1);
    const QRectF highlightRect(QPointF(0.2, 0.1), QSizeF(0.7, 0.7));
    calculatePreviewRects(resourceAr, highlightRect, targetAr, cropRect, croppedHighlightRect);
    ASSERT_EQ(cropRect, QRectF(QPointF(0.0, 0.1), QSizeF(1.0, 0.7)));
    ASSERT_EQ(croppedHighlightRect, QRectF(QPointF(0.2, 0.0), QSizeF(0.7, 1.0)));
}

} // namespace test
} // namespace nx::vms::client::core
