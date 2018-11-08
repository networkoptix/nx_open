#pragma once

#include <QtCore/QAbstractItemModel>

#include <nx/vms/client/desktop/common/delegates/widget_table_delegate.h>

class QnIoPortItemDelegate: public nx::vms::client::desktop::WidgetTableDelegate
{
    using base_type = nx::vms::client::desktop::WidgetTableDelegate;

public:
    using base_type::base_type; //< Forward constructors.

    virtual QWidget* createWidget(QAbstractItemModel* model,
        const QModelIndex& index, QWidget* parent) const override;

    virtual bool updateWidget(QWidget* widget, const QModelIndex& index) const override;

    virtual QSize sizeHint(QWidget* widget, const QModelIndex& index) const override;
};
