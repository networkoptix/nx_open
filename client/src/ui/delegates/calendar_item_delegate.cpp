#include "calendar_item_delegate.h"

#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/color_transformations.h>

#include <recording/time_period.h>
#include <recording/time_period_storage.h>

#include <ui/style/globals.h>


namespace {
    const QColor selectionColor = withAlpha(qnGlobals->selectionColor(), 192);
    
    const QColor primaryRecordingColor(32, 128, 32, 255);
    const QColor secondaryRecordingColor(32, 255, 32, 255);

    const QColor primaryMotionColor(128, 0, 0, 255);
    const QColor secondaryMotionColor(255, 0, 0, 255);

    const QColor separatorColor(0, 0, 0, 255);

    QColor primaryColor(QnCalendarItemDelegate::FillType fillType) {
        switch(fillType) {
        case QnCalendarItemDelegate::MotionFill:    return primaryMotionColor;
        case QnCalendarItemDelegate::RecordingFill: return primaryRecordingColor;
        default:                                    return QColor();
        }
    }

    QColor secondaryColor(QnCalendarItemDelegate::FillType fillType) {
        switch(fillType) {
        case QnCalendarItemDelegate::MotionFill:    return secondaryMotionColor;
        case QnCalendarItemDelegate::RecordingFill: return secondaryRecordingColor;
        default:                                    return QColor();
        }
    }

    QnCalendarItemDelegate::FillType fillType(const QnTimePeriod &period, const QnTimePeriodStorage &periodStorage) {
        if(periodStorage.periods(Qn::MotionContent).intersects(period)) {
            return QnCalendarItemDelegate::MotionFill;
        } else if(periodStorage.periods(Qn::RecordingContent).intersects(period)) {
            return QnCalendarItemDelegate::RecordingFill;
        } else {
            return QnCalendarItemDelegate::EmptyFill;
        }
    }

} // anonymous namespace

QnCalendarItemDelegate::QnCalendarItemDelegate(QObject *parent):
    base_type(parent) 
{}

void QnCalendarItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    base_type::paint(painter, option, index);
}

void QnCalendarItemDelegate::paintCell(QPainter *painter, const QPalette &palette, const QRect &rect, const QnTimePeriod &period, const QnTimePeriod &enabledRange, const QnTimePeriod &selectedRange, const QnTimePeriodStorage &primaryPeriods, const QnTimePeriodStorage &secondaryPeriods, const QString &text) const {
    paintCell(
        painter, 
        palette, 
        rect,
        enabledRange.intersects(period),
        selectedRange.intersects(period),
        fillType(period, primaryPeriods),
        fillType(period, secondaryPeriods),
        text
    );
}

void QnCalendarItemDelegate::paintCell(QPainter *painter, const QPalette &palette, const QRect &rect, bool isEnabled, bool isSelected, FillType primaryFill, FillType secondaryFill, const QString &text) const {
    QnScopedPainterBrushRollback brushRollback(painter);
    QnScopedPainterPenRollback penRollback(painter);
    QnScopedPainterFontRollback fontRollback(painter);

    /* Draw background. */
    if(primaryFill != EmptyFill)
        painter->fillRect(rect, QBrush(primaryColor(primaryFill), Qt::SolidPattern));
    if(secondaryFill != EmptyFill && secondaryFill != primaryFill)
        painter->fillRect(rect, QBrush(secondaryColor(secondaryFill), Qt::Dense6Pattern));

    /* Selection frame. */
    if (isSelected) {
        painter->setPen(QPen(selectionColor, 3, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(rect.adjusted(2, 2, -2, -2));
    }

    /* Common black frame. */
    painter->setPen(QPen(separatorColor, 1));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect);

    /* Text. */
    QFont font = painter->font();
    QColor color;
    if (!isEnabled) {
        color = palette.color(QPalette::Disabled, QPalette::Text);
    } else {
        color = palette.color(QPalette::Active, QPalette::Text);
        font.setBold(true);
    }

    painter->setPen(QPen(color, 1));
    painter->setFont(font);
    painter->drawText(rect, Qt::AlignCenter, text);
}
