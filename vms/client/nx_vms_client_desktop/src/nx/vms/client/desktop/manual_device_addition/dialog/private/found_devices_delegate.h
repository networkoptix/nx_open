// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
