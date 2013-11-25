#ifndef QN_CALENDAR_ITEM_DELEGATE_H
#define QN_CALENDAR_ITEM_DELEGATE_H

#include <QtCore/QMetaType>
#include <QtGui/QStyledItemDelegate>

#include <utils/common/json.h>

#include <ui/customization/customized.h>

class QnTimePeriod;
class QnTimePeriodStorage;

struct QnCalendarItemDelegateColors {
    QnCalendarItemDelegateColors();

    QColor selection;
    QColor primaryRecording;
    QColor secondaryRecording;
    QColor primaryMotion;
    QColor secondaryMotion;
    QColor separator;

    QColor primary(int fillType) const;
    QColor secondary(int fillType) const;
};

QN_DEFINE_STRUCT_SERIALIZATION_FUNCTIONS_OPTIONAL(QnCalendarItemDelegateColors, 
    (selection)(primaryRecording)(secondaryRecording)(primaryMotion)(secondaryMotion)(separator), inline)

Q_DECLARE_METATYPE(QnCalendarItemDelegateColors)


class QnCalendarItemDelegate: public Customized<QStyledItemDelegate> {
    Q_OBJECT
    Q_PROPERTY(QnCalendarItemDelegateColors colors READ colors WRITE setColors)
    typedef Customized<QStyledItemDelegate> base_type;

public:
    enum FillType {
        EmptyFill,
        RecordingFill,
        MotionFill
    };

    QnCalendarItemDelegate(QObject *parent = NULL);

    const QnCalendarItemDelegateColors &colors() const;
    void setColors(const QnCalendarItemDelegateColors &colors);

    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    
    void paintCell(QPainter *painter, const QPalette &palette, const QRect &rect, const QnTimePeriod &period, qint64 localOffset, const QnTimePeriod &enabledRange, const QnTimePeriod &selectedRange, const QnTimePeriodStorage &primaryPeriods, const QnTimePeriodStorage &secondaryPeriods, const QString &text) const;
    void paintCell(QPainter *painter, const QPalette &palette, const QRect &rect, bool isEnabled, bool isSelected, FillType primaryFill, FillType secondaryFill, const QString &text) const;

private:
    QnCalendarItemDelegateColors m_colors;
};

#endif // QN_CALENDAR_ITEM_DELEGATE_H

