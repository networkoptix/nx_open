#include "calendar_item_delegate.h"

#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/color_transformations.h>

#include <recording/time_period.h>
#include <recording/time_period_storage.h>

#include <ui/style/globals.h>


namespace 
{
    enum FillType 
    {
        kRecordingFill   = 0x1,
        kBookmarkFill    = 0x2,
        kMotionFill      = 0x4
    };

    const Qt::BrushStyle kPrimaryBrushStyle = Qt::SolidPattern;
    const Qt::BrushStyle kSecondaryBrushStyle = Qt::Dense5Pattern;
    const QPoint kRightOffset = QPoint(1, 0);
    const QPoint kBottomOffset = QPoint(0, 1);
    const QPoint kBottomRightOffset = kRightOffset + kBottomOffset;

    int fillType(const QnTimePeriod &period, const QnTimePeriodStorage &periodStorage) 
    {
        enum { kNoFlag = 0 };
        return ((periodStorage.periods(Qn::MotionContent).intersects(period) ? kMotionFill : kNoFlag)
            | (periodStorage.periods(Qn::RecordingContent).intersects(period) ? kRecordingFill : kNoFlag)
            | (periodStorage.periods(Qn::BookmarksContent).intersects(period) ? kBookmarkFill : kNoFlag));
    }

    bool paintBackground(int fillType
        , bool isPrimary
        , const QRect &rect
        , const QnCalendarColors &colors
        , QPainter &painter)
    {
        const bool isRecording = (fillType & kRecordingFill);
        if (isRecording || (fillType & kBookmarkFill))
        {
            const Qt::BrushStyle style = (isPrimary ? kPrimaryBrushStyle : kSecondaryBrushStyle);
            const QBrush brush(colors.getBackground(fillType, isPrimary), style);
    
            painter.setPen(Qt::NoPen);
            painter.fillRect(rect, brush);
            return true;
        }
        return false;
    }

} // anonymous namespace


QColor QnCalendarColors::getBackground(int fillType
    , bool isPrimary) const
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

QColor QnCalendarColors::getMotionBackground(bool isPrimary) const
{
    return (isPrimary ? primaryMotion : secondaryMotion);
}

QnCalendarItemDelegate::QnCalendarItemDelegate(QObject *parent):
    base_type(parent) 
{}

const QnCalendarColors &QnCalendarItemDelegate::colors() const {
    return m_colors;
}

void QnCalendarItemDelegate::setColors(const QnCalendarColors &colors) {
    m_colors = colors;
}

void QnCalendarItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    base_type::paint(painter, option, index);
}

void QnCalendarItemDelegate::paintCell(QPainter *painter, const QPalette &palette, const QRect &rect
    , const QnTimePeriod &period, qint64 localOffset, const QnTimePeriod &enabledRange
    , const QnTimePeriod &selectedRange, const QnTimePeriodStorage &primaryPeriods
    , const QnTimePeriodStorage &secondaryPeriods, const QString &text) const 
{
    assert(painter);
    
    const QnTimePeriod localPeriod(period.startTimeMs - localOffset, period.durationMs);
    const bool isEnabled = enabledRange.intersects(localPeriod);
    const bool isSelected = selectedRange.intersects(localPeriod);
    const int primaryFill = fillType(localPeriod, primaryPeriods);
    const int secondaryFill = fillType(localPeriod, secondaryPeriods);
    
    const QnScopedPainterBrushRollback brushRollback(painter);
    const QnScopedPainterPenRollback penRollback(painter);
    const QnScopedPainterFontRollback fontRollback(painter);

    /* Draws the background - could be one of the recording or bookmark fill types*/
    if (!paintBackground(primaryFill, true, rect, m_colors, *painter))
        paintBackground(secondaryFill, false, rect, m_colors, *painter);

    /* Draws motion mark */
    const bool isPrimaryMotion = (primaryFill & kMotionFill);
    if (isPrimaryMotion || (secondaryFill & kMotionFill))
    {
        QPolygon poly;
        poly.append(rect.bottomLeft() + kBottomOffset);
        poly.append(rect.topRight() + kRightOffset);
        poly.append(rect.bottomRight() + kBottomRightOffset);

        painter->setPen(Qt::NoPen);
        painter->setBrush(QBrush(m_colors.getMotionBackground(isPrimaryMotion)
            , (isPrimaryMotion ? kPrimaryBrushStyle : kSecondaryBrushStyle)));
        painter->drawPolygon(poly, Qt::WindingFill);
    }

    /* Selection frame. */
    if (isSelected) 
    {
        enum { kOffset = 2, kWidth = 3 };
        painter->setPen(QPen(m_colors.selection, kWidth, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(rect.adjusted(kOffset, kOffset, -kOffset, -kOffset));
    }

    /* Common black frame. */
    enum { kPenWidth = 1 };
    painter->setPen(QPen(m_colors.separator, kPenWidth));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect);

    /* Text. */
    const QColor color = (isEnabled ? palette.color(QPalette::Active, QPalette::Text)
        : palette.color(QPalette::Disabled, QPalette::Text));
    QFont font = painter->font();
    font.setBold(isEnabled);

    painter->setPen(QPen(color, kPenWidth));
    painter->setFont(font);
    painter->drawText(rect, Qt::AlignCenter, text);
}
