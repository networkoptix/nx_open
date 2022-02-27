// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>

#include <core/resource_access/resource_access_subject.h>
#include <nx/vms/client/desktop/resource_dialogs/item_delegates/resource_selection_dialog_item_delegate.h>

namespace nx::core::access { class ResourceAccessProvider; }

namespace nx::vms::client::desktop {

class AccessibleMediaDialogItemDelegate: public ResourceSelectionDialogItemDelegate
{
    Q_OBJECT
    using base_type = ResourceSelectionDialogItemDelegate;

public:
    AccessibleMediaDialogItemDelegate(nx::core::access::ResourceAccessProvider* accessProvider,
        QObject* parent = nullptr);

    virtual void paint(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const override;

    void setSubject(const QnResourceAccessSubject& subject);

protected:
    virtual int decoratorWidth(const QModelIndex& index) const override;

private:
    QPointer<nx::core::access::ResourceAccessProvider> m_resourceAccessProvider;
    QnResourceAccessSubject m_subject;
};

} // namespace nx::vms::client::desktop
