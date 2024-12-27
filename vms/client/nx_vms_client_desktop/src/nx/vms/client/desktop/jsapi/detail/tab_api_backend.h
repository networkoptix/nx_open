// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

#include "tab_structures.h"

class QnWorkbenchLayout;
class QnWorkbenchContext;

namespace nx::vms::client::desktop {

class WindowContext;

namespace jsapi::detail {

/** Class implements management functions for the opened tab. */
class TabApiBackend: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    /**
     * Creates backend for the management functions and watches for the tab with the specified
     * layout.
     * @param context Context to be used for the access to the workbench functions.
     * @param layout Layout to be watched for.
     * @param parent Parent.
     */
    TabApiBackend(WindowContext* context, QnWorkbenchLayout* layout, QObject* parent = nullptr);
    virtual ~TabApiBackend() override;

    State state() const;
    ItemResult item(const QUuid& itemId) const;
    ItemResult addItem(
        const ResourceUniqueId& resourceId,
        const ItemParams& params);

    Error setItemParams(
        const QUuid& itemId,
        const ItemParams& params);

    Error setMaximizedItem(const QString& itemId);
    Error removeItem(const QUuid& itemId);
    Error syncWith(const QUuid& itemId);
    Error stopSyncPlay();
    Error setLayoutProperties(const LayoutProperties& properties);
    Error saveLayout();

    QnWorkbenchLayout* layout() const;

signals:
    void itemAdded(const Item& item);
    void itemRemoved(const nx::Uuid& itemId);
    void itemChanged(const Item& item);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace jsapi::detail
} // namespace nx::vms::client::desktop
