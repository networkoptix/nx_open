// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "resource_dialog_item_delegate_base.h"

namespace nx::vms::client::desktop {

class ResourceDialogItemDelegate: public ResourceDialogItemDelegateBase
{
    Q_OBJECT
    using base_type = ResourceDialogItemDelegateBase;

public:
    ResourceDialogItemDelegate(QObject* parent = nullptr);

    bool showRecordingIndicator() const;
    void setShowRecordingIndicator(bool show);

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

    void paintItemText(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const;

    void paintItemIcon(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index,
        QIcon::Mode mode) const;

    void paintRecordingIndicator(QPainter* painter,
        const QRect& iconRect,
        const QModelIndex& index) const;

private:
    bool m_showRecordingIndicator = false;
};

} // namespace nx::vms::client::desktop
