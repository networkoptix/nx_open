// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/vms/client/desktop/resource_dialogs/item_delegates/resource_dialog_item_delegate.h>

namespace nx::vms::client::desktop {

class ResourceSelectionDialogItemDelegate: public ResourceDialogItemDelegate
{
    Q_OBJECT
    using base_type = ResourceDialogItemDelegate;

public:
    ResourceSelectionDialogItemDelegate(QObject* parent = nullptr);

    enum CheckMarkType
    {
        CheckBox,
        RadioButton,
    };
    CheckMarkType checkMarkType() const;
    void setCheckMarkType(CheckMarkType type);

    virtual void paint(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const override;

protected:
    virtual std::optional<Qt::CheckState> itemCheckState(
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const;

    virtual void initStyleOption(
        QStyleOptionViewItem* option,
        const QModelIndex& index) const override;

    virtual int decoratorWidth(const QModelIndex& index) const override;

    void paintItemCheckMark(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index,
        const QColor& checkedColor) const;

private:
    CheckMarkType m_checkMarkType = CheckBox;
};

} // namespace nx::vms::client::desktop
