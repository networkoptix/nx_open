// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

#include "tab_structures.h"

class QnWorkbenchLayout;
class QnWorkbenchContext;

namespace nx::vms::client::desktop::jsapi::detail {

/** Class implements management functions for the opened tab. */
class TabApiBackend: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    /**
     * Creates backend for the management functions and watchs for the tab with the specified
     * layout.
     * @param context Context to be used for the access to the workbench functions.
     * @param layout Layout to be watched for.
     */
    TabApiBackend(
        QnWorkbenchContext* context,
        QnWorkbenchLayout* layout,
        QObject* parent = nullptr);

    virtual ~TabApiBackend() override;

    /**
     * Full state of the watched tab. States contains information about all items and corresponding
     * layout.
     * @see State
     */
    State state() const;

    /** Description of the item with specified id or error if it can't be found. */
    ItemResult item(const QUuid& itemId) const;

    /** Add item for the specified resource and initializes it with the specified parameters. */
    ItemResult addItem(
        const ResourceUniqueId& resourceId,
        const ItemParams& params);

    /** Change specified parameters for the item with the selected id. */
    Error setItemParams(
        const QUuid& itemId,
        const ItemParams& params);

    /** Remove workbench item with specified id from the tab and corresponding layout. */
    Error removeItem(const QUuid& itemId);

    /**
     * Make all items synced with the specified item. All items on the tab
     * will have the same timestamp,playing state and speed.
     */
    Error syncWith(const QUuid& itemId);

    /** Stop syncing of the corresponding layout. */
    Error stopSyncPlay();

    /** Set properties for the corresponding layout. */
    Error setLayoutProperties(const LayoutProperties& properties);

    /** Save layout. */
    Error saveLayout();

signals:
    void itemAdded(const Item& item);
    void itemRemoved(const QnUuid& itemId);
    void itemChanged(const Item& item);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::jsapi::detail
