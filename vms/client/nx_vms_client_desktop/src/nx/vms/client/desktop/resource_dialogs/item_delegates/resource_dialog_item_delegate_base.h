// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QStyledItemDelegate>

namespace nx::vms::client::desktop {

/**
 * Base class for all item delegate implementations related to the resource dialogs.
 */
class ResourceDialogItemDelegateBase: public QStyledItemDelegate
{
    Q_OBJECT
    using base_type = QStyledItemDelegate;

public:
    ResourceDialogItemDelegateBase(QObject* parent = nullptr);

    static bool isSpacer(const QModelIndex& index);
    static bool isSeparator(const QModelIndex& index);

    virtual QSize sizeHint(
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

    /**
     * Base implementation of the <tt>paint()</tt> method provides display of spacer and separator
     * items. Since any further drawing operations aren't expected for that types of items this
     * method is finalized to avoid redundant item type checks in the subclasses. For any other
     * item type <tt>paintContents()</tt> virtual method will be called with same arguments passed.
     */
    virtual void paint(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const override final;

    /**
     * Any non-spacer, non-separator item drawing should be implemented by overriding this method
     * in subclasses. Base implementation just performs <tt>QStyledItemDelegate::paint()</tt> call.
     */
     virtual void paintContents(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const;

protected:
    virtual void initStyleOption(
        QStyleOptionViewItem* option,
        const QModelIndex& index) const override;

    void paintSeparator(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const;
};

} // namespace nx::vms::client::desktop
