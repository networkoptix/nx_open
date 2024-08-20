// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

#include "detail/tab_structures.h"
#include "globals.h"

class QnWorkbenchLayout;

namespace nx::vms::client::desktop {

class WindowContext;

namespace jsapi {

namespace detail { class TabApiBackend; }

/**
 * Contains methods and signals to work with the tab.
 */
class Tab: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    /**
     * @addtogroup vms-tab
     * Contains methods and signals to work with the tab containing the current web page. If the
     * web page is opened in a dedicated window, @ref vms-tabs should be used.
     * @{
     */

    /**
     * Name of the tab.
     */
    Q_PROPERTY(QString name READ name CONSTANT)

    /**
     * Id of the tab.
     */
    Q_PROPERTY(QString id READ id CONSTANT)

public:
    /** @private */
    Tab(
        WindowContext* context,
        QnWorkbenchLayout* layout,
        std::function<bool()> isSupportedCondition = {},
        QObject* parent = nullptr);

    /** @private */
    virtual ~Tab() override;

    /** @return Complete tab state. */
    Q_INVOKABLE State state() const;

    /** @return Description of an item with the specified identifier. */
    Q_INVOKABLE ItemResult item(const QUuid& itemId) const;

    /**
      * Adds an item from the Resource with the specified identifier and item parameters.
      * @return Item description on success.
     */
    Q_INVOKABLE ItemResult addItem(const QString& resourceId, const ItemParams& params);

    /** Sets parameters for the item with the specified identifier. */
    Q_INVOKABLE Error setItemParams(const QUuid& itemId, const ItemParams& params);

    /** Removes the item with the specified identifier. */
    Q_INVOKABLE Error removeItem(const QUuid& itemId);

    /**
     * Makes all items synced with the specified item. All items on the tab will have the same
     * timestamp / playing state and speed.
     */
    Q_INVOKABLE Error syncWith(const QUuid& itemId);

    /** Stops syncing of the corresponding Layout. */
    Q_INVOKABLE Error stopSyncPlay();

    /** Sets the specified properties for the corresponding Layout. */
    Q_INVOKABLE Error setLayoutProperties(const LayoutProperties& properties);

    /** Saves the Layout. */
    Q_INVOKABLE Error saveLayout();

    /** @private */
    QString id() const;

    /** @private */
    QString name() const;

    /** @private */
    QnWorkbenchLayout* layout() const;

signals:
    void itemAdded(const Item& item);
    void itemRemoved(const nx::Uuid& itemId);
    void itemChanged(const Item& item);

    /** @} */ // group vms-tab

private:
    bool ensureSupported() const;

private:
    nx::utils::ImplPtr<detail::TabApiBackend> d;
    std::function<bool()> m_isSupportedCondition;
};

} // namespace jsapi
} // namespace nx::vms::client::desktop
