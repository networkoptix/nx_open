// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource_dialogs/item_delegates/resource_dialog_item_delegate_base.h>

namespace nx::vms::client::desktop {

class IndirectAccessColumnItemDelegate: public ResourceDialogItemDelegateBase
{
    Q_OBJECT
    using base_type = ResourceDialogItemDelegateBase;

public:
    IndirectAccessColumnItemDelegate(QObject* parent = nullptr);

    virtual void paintContents(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const override;
};

} // namespace nx::vms::client::desktop
