// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource_dialogs/item_delegates/resource_dialog_item_delegate_base.h>
#include <nx/vms/api/types/resource_types.h>

class QTreeView;

namespace nx::vms::client::desktop {

class FailoverPriorityColumnItemDelegate: public ResourceDialogItemDelegateBase
{
    Q_OBJECT
    using base_type = ResourceDialogItemDelegateBase;

public:
    FailoverPriorityColumnItemDelegate(QObject* parent = nullptr);

protected:
    virtual void initStyleOption(
        QStyleOptionViewItem* option,
        const QModelIndex& index) const override;

private:
    QString failoverPriorityToString(nx::vms::api::FailoverPriority failoverPriority) const;
    QColor failoverPriorityToColor(nx::vms::api::FailoverPriority failoverPriority) const;
};

} // namespace nx::vms::client::desktop
