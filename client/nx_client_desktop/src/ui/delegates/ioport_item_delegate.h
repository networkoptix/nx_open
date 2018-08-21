#pragma once

#include <QtCore/QAbstractItemModel>

#include <nx/client/desktop/common/delegates/widget_table_delegate.h>

class QnIoPortItemDelegate: public nx::client::desktop::WidgetTableDelegate
{
    using base_type = nx::client::desktop::WidgetTableDelegate;

public:
    using base_type::base_type; //< Forward constructors.

    virtual QWidget* createWidget(QAbstractItemModel* model,
        const QModelIndex& index, QWidget* parent) const override;

    virtual bool updateWidget(QWidget* widget, const QModelIndex& index) const override;

    virtual QSize sizeHint(QWidget* widget, const QModelIndex& index) const override;
};
