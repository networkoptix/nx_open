// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "calendar_item_delegate.h"

#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/color_transformations.h>

#include <recording/time_period.h>

#include <nx/vms/client/core/media/time_period_storage.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

using namespace nx::vms::client::desktop;

using nx::vms::client::core::TimePeriodStorage;

namespace {

enum FillType
{
    kNoFill          = 0x0,
    kRecordingFill   = 0x1,
    kBookmarkFill    = 0x2,
    kMotionFill      = 0x4
};

struct Colors
{
    QColor primaryRecording = colorTheme()->color("calendar.cell.primaryRecording");
    QColor secondaryRecording = colorTheme()->color("calendar.cell.secondaryRecording");
    QColor primaryBookmark = colorTheme()->color("calendar.cell.primaryBookmark");
    QColor secondaryBookmark = colorTheme()->color("calendar.cell.secondaryBookmark");

    QColor getBackground(int fillType, bool isPrimary) const
    {
        /// Note! Bookmark priority is higher than recording
        if (fillType & kBookmarkFill)
        {
            return (isPrimary ? primaryBookmark : secondaryBookmark);
        }
        else if (fillType & kRecordingFill)
        {
            return (isPrimary ? primaryRecording : secondaryRecording);
        }
        return QColor();
    }
};

static const Qt::BrushStyle kPrimaryBrushStyle = Qt::SolidPattern;
static const Qt::BrushStyle kSecondaryBrushStyle = Qt::Dense5Pattern;
static const QPoint kRightOffset = QPoint(1, 0);
static const QPoint kBottomOffset = QPoint(0, 1);
static const QPoint kBottomRightOffset = kRightOffset + kBottomOffset;

int fillType(const QnTimePeriod &period, const TimePeriodStorage &periodStorage)
{
    return ((periodStorage.periods(Qn::MotionContent).intersects(period) ? kMotionFill : kNoFill)
        | (periodStorage.periods(Qn::RecordingContent).intersects(period) ? kRecordingFill : kNoFill));
    // TODO: #dklychkov Reimplement it taking bookmarks from query
        //| (periodStorage.periods(Qn::BookmarksContent).intersects(period) ? kBookmarkFill : kNoFill));
}

bool paintBackground(int fillType,
    bool isPrimary,
    const QRect& rect,
    QPainter* painter)
{
    static const Colors kColors;

    const bool isRecording = (fillType & kRecordingFill);
    if (isRecording || (fillType & kBookmarkFill))
    {
        const Qt::BrushStyle style = (isPrimary ? kPrimaryBrushStyle : kSecondaryBrushStyle);
        const QBrush brush(kColors.getBackground(fillType, isPrimary), style);

        painter->setPen(Qt::NoPen);
        painter->fillRect(rect, brush);
        return true;
    }
    return false;
}

} // namespace

QnCalendarItemDelegate::QnCalendarItemDelegate(QObject *parent):
    base_type(parent)
{}

void QnCalendarItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    base_type::paint(painter, option, index);
}

void QnCalendarItemDelegate::paintCell(QPainter* painter,
    const QRect& rect,
    const QnTimePeriod& localPeriod,
    const TimePeriodStorage& primaryPeriods,
    const TimePeriodStorage& secondaryPeriods,
    bool isSelected) const
{
    static const QBrush kPrimaryMotionBrush(
        colorTheme()->color("calendar.cell.primaryMotion"),
        kPrimaryBrushStyle);

    static const QBrush kSecondaryMotionBrush(
        colorTheme()->color("calendar.cell.secondaryMotion"),
        kSecondaryBrushStyle);

    static const QPen kSelectionPen(
        colorTheme()->color("calendar.cell.selection"),
        /*width*/ 3,
        Qt::SolidLine,
        Qt::SquareCap,
        Qt::MiterJoin);

    static const QPen kSeparatorPen(
        colorTheme()->color("calendar.cell.separator"),
        /*width*/ 1);

    const QnScopedPainterBrushRollback brushRollback(painter);
    const QnScopedPainterPenRollback penRollback(painter);

    const int primaryFill = fillType(localPeriod, primaryPeriods);
    const int secondaryFill = fillType(localPeriod, secondaryPeriods);

    /* Draws the background - could be one of the recording or bookmark fill types*/
    if (!paintBackground(primaryFill, true, rect, painter))
        paintBackground(secondaryFill, false, rect, painter);

    /* Draws motion mark */
    const bool isPrimaryMotion = (primaryFill & kMotionFill);
    if (isPrimaryMotion || (secondaryFill & kMotionFill))
    {
        QPolygon poly;
        poly.append(rect.bottomLeft() + kBottomOffset);
        poly.append(rect.topRight() + kRightOffset);
        poly.append(rect.bottomRight() + kBottomRightOffset);

        painter->setPen(Qt::NoPen);

        painter->setBrush(isPrimaryMotion ? kPrimaryMotionBrush : kSecondaryMotionBrush);
        painter->drawPolygon(poly, Qt::WindingFill);
    }

    /* Selection frame. */
    if (isSelected)
    {
        enum { kOffset = 2 };
        painter->setPen(kSelectionPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(rect.adjusted(kOffset, kOffset, -kOffset, -kOffset));
    }

    /* Common black frame. */
    painter->setPen(kSeparatorPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect);
}

void QnCalendarItemDelegate::paintCellText(QPainter* painter,
    const QPalette& palette,
    const QRect& rect,
    const QString& text,
    bool isEnabled,
    QPalette::ColorRole foregroundRole) const
{
    const QnScopedPainterPenRollback penRollback(painter);
    const QnScopedPainterFontRollback fontRollback(painter);

    const QColor color = (isEnabled
        ? palette.color(QPalette::Active, foregroundRole)
        : palette.color(QPalette::Disabled, QPalette::Text));

    QFont font = painter->font();
    font.setBold(isEnabled);

    enum { kPenWidth = 1 };
    painter->setPen(QPen(color, kPenWidth));
    painter->setFont(font);
    painter->drawText(rect, Qt::AlignCenter, text);
}
