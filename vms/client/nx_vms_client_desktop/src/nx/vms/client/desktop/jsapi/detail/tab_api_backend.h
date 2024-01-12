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
    TabApiBackend(
        WindowContext* context,
        QnWorkbenchLayout* layout,
        QObject* parent = nullptr);

    virtual ~TabApiBackend() override;

    /**
     * @addtogroup vms-tab
     * Contains methods and signals to work with the tab containing the current web page.
     * @{
     */

    /** @return Complete tab state. */
    State state() const;

    /** @return Description of an item with the specified identifier. */
    ItemResult item(const QUuid& itemId) const;

    /**
      * Adds an item from the Resource with the specified identifier and item parameters.
      * @return Item description on success.
     */
    ItemResult addItem(
        const ResourceUniqueId& resourceId,
        const ItemParams& params);

    /** Sets parameters for the item with the specified identifier. */
    Error setItemParams(
        const QUuid& itemId,
        const ItemParams& params);

    /** Removes the item with the specified identifier. */
    Error removeItem(const QUuid& itemId);

    /**
     * Makes all items synced with the specified item. All items on the tab will have the same
     * timestamp / playing state and speed.
     */
    Error syncWith(const QUuid& itemId);

    /** Stops syncing of the corresponding Layout. */
    Error stopSyncPlay();

    /** Sets the specified properties for the corresponding Layout. */
    Error setLayoutProperties(const LayoutProperties& properties);

    /** Saves the Layout. */
    Error saveLayout();

signals:
    void itemAdded(const Item& item);
    void itemRemoved(const QnUuid& itemId);
    void itemChanged(const Item& item);

    /** @} */ // group vms-tab

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace jsapi::detail
} // namespace nx::vms::client::desktop
