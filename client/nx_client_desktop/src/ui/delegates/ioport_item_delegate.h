#pragma once

#include <QtCore/QAbstractItemModel>

#include <ui/widgets/common/widget_table_delegate.h>

class QnIoPortItemDelegate: public QnWidgetTableDelegate
{
    using base_type = QnWidgetTableDelegate;

public:
    using base_type::QnWidgetTableDelegate; //< forward constructor

    virtual QWidget* createWidget(QAbstractItemModel* model,
        const QModelIndex& index, QWidget* parent) const override;

    virtual bool updateWidget(QWidget* widget, const QModelIndex& index) const override;

    virtual QSize sizeHint(QWidget* widget, const QModelIndex& index) const override;
};
