#ifndef QN_CALENDAR_CELL_PAINTER_H
#define QN_CALENDAR_CELL_PAINTER_H

#include <QStyledItemDelegate>

class QnTimePeriod;
class QnTimePeriodStorage;

class QnCalendarItemDelegate: public QStyledItemDelegate {
    Q_OBJECT
    typedef QStyledItemDelegate base_type;

public:
    enum FillType {
        EmptyFill,
        RecordingFill,
        MotionFill,
    };

    QnCalendarItemDelegate(QObject *parent = NULL);

    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    
    void paintCell(QPainter *painter, const QPalette &palette, const QRect &rect, const QnTimePeriod &period, qint64 localOffset, const QnTimePeriod &enabledRange, const QnTimePeriod &selectedRange, const QnTimePeriodStorage &primaryPeriods, const QnTimePeriodStorage &secondaryPeriods, const QString &text) const;
    void paintCell(QPainter *painter, const QPalette &palette, const QRect &rect, bool isEnabled, bool isSelected, FillType primaryFill, FillType secondaryFill, const QString &text) const;
};

#endif QN_CALENDAR_CELL_PAINTER_H

