// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QPixmap>

#include <nx/vms/client/desktop/resource_dialogs/item_delegates/resource_dialog_item_delegate.h>

namespace nx::vms::client::desktop {

class ItemViewHoverTracker;

class BackupSettingsItemDelegate: public ResourceDialogItemDelegate
{
    Q_OBJECT
    using base_type = ResourceDialogItemDelegate;

public:
    enum class DropdownStateFlag
    {
        none = 0x0,
        enabled = 0x1,
        hovered = 0x2,
        selected = 0x4,
    };
    Q_ENUM(DropdownStateFlag)
    Q_DECLARE_FLAGS(DropdownStateFlags, DropdownStateFlag)

    BackupSettingsItemDelegate(ItemViewHoverTracker* hoverTracker, QObject* parent = nullptr);

    // Index representing dropdown item for which menu is visible.
    void setActiveDropdownIndex(const QModelIndex& index);

    virtual QSize sizeHint(
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

    virtual void paintContents(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const override;

protected:
    virtual void initStyleOption(
        QStyleOptionViewItem* option,
        const QModelIndex& index) const override;

private:
    void paintWarningIcon(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const;

    void paintDropdown(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const;

    void paintSwitch(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const;

private:
    DropdownStateFlags dropdownStateFlags(
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const;

    void initDropdownColorTable();

private:
    ItemViewHoverTracker* m_hoverTracker = nullptr;
    QHash<DropdownStateFlags, QColor> m_dropdownColorTable;
    QPixmap m_warningPixmap;
    QModelIndex m_activeDropdownIndex;
    mutable QHash<QString, int> m_dropdownTextWidthCache;
};

} // namespace nx::vms::client::desktop
