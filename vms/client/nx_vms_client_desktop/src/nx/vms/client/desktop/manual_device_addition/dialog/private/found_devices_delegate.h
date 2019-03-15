#pragma once

#include <QtWidgets/QStyledItemDelegate>

namespace nx::vms::client::desktop {

class FoundDevicesDelegate: public QStyledItemDelegate
{
    Q_OBJECT
    using base_type = QStyledItemDelegate;

public:
    FoundDevicesDelegate(QObject* parent = nullptr);

    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;
};

} // namespace nx::vms::client::desktop
