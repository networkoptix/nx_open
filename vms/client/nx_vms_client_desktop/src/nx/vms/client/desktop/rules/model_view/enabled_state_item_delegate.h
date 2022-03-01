// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/common/delegates/switch_item_delegate.h>

namespace nx::vms::client::desktop::rules {

class EnabledStateItemDelegate: public SwitchItemDelegate
{
    Q_OBJECT

public:
    explicit EnabledStateItemDelegate(QObject* parent = nullptr);

    virtual QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

    virtual void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    virtual void setModelData(QWidget* editor, QAbstractItemModel* model,
        const QModelIndex& index) const override;

    virtual void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

private:
    void commitAndClose();
};

} // namespace nx::vms::client::desktop:rules
