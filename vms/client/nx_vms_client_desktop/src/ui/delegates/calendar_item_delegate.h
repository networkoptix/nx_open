// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_CALENDAR_ITEM_DELEGATE_H
#define QN_CALENDAR_ITEM_DELEGATE_H

#include <QtCore/QMetaType>
#include <QtWidgets/QStyledItemDelegate>

struct QnTimePeriod;
class QnTimePeriodStorage;

class QnCalendarItemDelegate: public QStyledItemDelegate
{
    Q_OBJECT
    using base_type = QStyledItemDelegate;

public:
    QnCalendarItemDelegate(QObject* parent = nullptr);

    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    void paintCell(QPainter* painter,
        const QRect& rect,
        const QnTimePeriod& localPeriod,
        const QnTimePeriodStorage& primaryPeriods,
        const QnTimePeriodStorage& secondaryPeriods,
        bool isSelected) const;

    void paintCellText(QPainter* painter,
        const QPalette& palette,
        const QRect& rect,
        const QString& text,
        bool isEnabled,
        QPalette::ColorRole foregroundRole = QPalette::Text) const;
};

#endif // QN_CALENDAR_ITEM_DELEGATE_H

