#pragma once

#include "found_devices_delegate.h"

namespace nx::vms::client::desktop {

class PresentedStateDelegate: public FoundDevicesDelegate
{
    Q_OBJECT
    using base_type = FoundDevicesDelegate;

public:
    PresentedStateDelegate(QObject* parent = nullptr);

    QWidget* createEditor(QWidget* parent,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

    void setEditorData(QWidget *editor,
        const QModelIndex &index) const override;
};

} // namespace nx::vms::client::desktop
