// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QStyledItemDelegate>

namespace nx::vms::client::desktop::rules {

class ModificationMarkItemDelegate: public QStyledItemDelegate
{
    Q_OBJECT

public:
    ModificationMarkItemDelegate(QObject* parent = nullptr);

    virtual void paint(QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

    virtual QSize sizeHint(const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;
};

} // namespace nx::vms::client::desktop:rules
