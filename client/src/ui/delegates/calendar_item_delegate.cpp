#include "calendar_item_delegate.h"

#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/color_transformations.h>

#include <recording/time_period.h>
#include <recording/time_period_storage.h>

#include <ui/style/globals.h>


namespace {
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


QColor QnCalendarColors::primary(int fillType) const {
    switch(fillType) {
    case QnCalendarItemDelegate::MotionFill:    return primaryMotion;
    case QnCalendarItemDelegate::RecordingFill: return primaryRecording;
    default:                                    return QColor();
    }
}

QColor QnCalendarColors::secondary(int fillType) const {
    switch(fillType) {
    case QnCalendarItemDelegate::MotionFill:    return secondaryMotion;
    case QnCalendarItemDelegate::RecordingFill: return secondaryRecording;
    default:                                    return QColor();
    }
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

void QnCalendarItemDelegate::paintCell(QPainter *painter, const QPalette &palette, const QRect &rect, const QnTimePeriod &period, qint64 localOffset, const QnTimePeriod &enabledRange, const QnTimePeriod &selectedRange, const QnTimePeriodStorage &primaryPeriods, const QnTimePeriodStorage &secondaryPeriods, const QString &text) const {
    QnTimePeriod localPeriod(period.startTimeMs - localOffset, period.durationMs);
    paintCell(
        painter, 
        palette, 
        rect,
        enabledRange.intersects(localPeriod),
        selectedRange.intersects(localPeriod),
        fillType(localPeriod, primaryPeriods),
        fillType(localPeriod, secondaryPeriods),
        text
    );
}

void QnCalendarItemDelegate::paintCell(QPainter *painter, const QPalette &palette, const QRect &rect, bool isEnabled, bool isSelected, FillType primaryFill, FillType secondaryFill, const QString &text) const {
    QnScopedPainterBrushRollback brushRollback(painter);
    QnScopedPainterPenRollback penRollback(painter);
    QnScopedPainterFontRollback fontRollback(painter);

    /* Draw background. */
    if(primaryFill != EmptyFill)
        painter->fillRect(rect, QBrush(m_colors.primary(primaryFill), Qt::SolidPattern));
    if(secondaryFill != EmptyFill && secondaryFill != primaryFill)
        painter->fillRect(rect, QBrush(m_colors.secondary(secondaryFill), Qt::Dense6Pattern));

    /* Selection frame. */
    if (isSelected) {
        painter->setPen(QPen(m_colors.selection, 3, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(rect.adjusted(2, 2, -2, -2));
    }

    /* Common black frame. */
    painter->setPen(QPen(m_colors.separator, 1));
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
