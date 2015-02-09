#ifndef QN_CALENDAR_ITEM_DELEGATE_H
#define QN_CALENDAR_ITEM_DELEGATE_H

#include <QtCore/QMetaType>
#include <QtWidgets/QStyledItemDelegate>

#include <client/client_color_types.h>

#include <ui/customization/customized.h>

class QnTimePeriod;
class QnTimePeriodStorage;


class QnCalendarItemDelegate: public Customized<QStyledItemDelegate> {
    Q_OBJECT
    Q_PROPERTY(QnCalendarColors colors READ colors WRITE setColors)
    typedef Customized<QStyledItemDelegate> base_type;

public:
    QnCalendarItemDelegate(QObject *parent = NULL);

    const QnCalendarColors &colors() const;
    void setColors(const QnCalendarColors &colors);

    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    
    void paintCell(QPainter *painter, const QPalette &palette, const QRect &rect
        , const QnTimePeriod &period, qint64 localOffset, const QnTimePeriod &enabledRange
        , const QnTimePeriod &selectedRange, const QnTimePeriodStorage &primaryPeriods
        , const QnTimePeriodStorage &secondaryPeriods, const QString &text) const;
   
private:
    QnCalendarColors m_colors;
};

#endif // QN_CALENDAR_ITEM_DELEGATE_H

