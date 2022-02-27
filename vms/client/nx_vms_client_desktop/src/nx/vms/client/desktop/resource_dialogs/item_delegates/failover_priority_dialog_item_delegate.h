// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource_dialogs/item_delegates/resource_dialog_item_delegate.h>
#include <nx/vms/api/types/resource_types.h>

class QTreeView;

namespace nx::vms::client::desktop {

class FailoverPriorityDialogItemDelegate: public ResourceDialogItemDelegate
{
    Q_OBJECT
    using base_type = ResourceDialogItemDelegate;

public:
    FailoverPriorityDialogItemDelegate(QObject* parent = nullptr);

protected:
    virtual void paint(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const override;

    virtual int decoratorWidth(const QModelIndex& index) const override;

private:
    QString failoverPriorityToString(nx::vms::api::FailoverPriority failoverPriority) const;
    QColor failoverPriorityToColor(nx::vms::api::FailoverPriority failoverPriority) const;

    void paintFailoverPriorityString(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const;
};

} // namespace nx::vms::client::desktop
