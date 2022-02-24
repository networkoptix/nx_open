// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource_dialogs/item_delegates/resource_dialog_item_delegate.h>

namespace nx::vms::client::desktop {

/**
 * Item delegate which has sole purpose to display checkboxes in item views. It takes in account
 * all required visual tweaks and also able to display radio button alike check marks for the cases
 * where exclusive selection is required.
 */
class CheckBoxColumnItemDelegate: public ResourceDialogItemDelegateBase
{
    Q_OBJECT
    using base_type = ResourceDialogItemDelegateBase;

public:
    CheckBoxColumnItemDelegate(QObject* parent = nullptr);

    enum CheckMarkType
    {
        CheckBox,
        RadioButton,
    };
    CheckMarkType checkMarkType() const;
    void setCheckMarkType(CheckMarkType type);

    virtual void paintContents(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const override;

protected:
    void paintItemCheckMark(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const;

private:
    CheckMarkType m_checkMarkType = CheckBox;
};

} // namespace nx::vms::client::desktop
