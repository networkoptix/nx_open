// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>

class QnWorkbenchItem;
class QnWorkbenchLayout;

/**
 * This class performs bidirectional synchronization of instances of
 * <tt>QnWorkbenchLayout</tt> and <tt>QnLayoutResource</tt>.
 */
class QnWorkbenchLayoutSynchronizer: public QObject
{
    Q_OBJECT
    typedef QObject base_type;
    using LayoutResourcePtr = nx::vms::client::desktop::LayoutResourcePtr;

public:
    QnWorkbenchLayoutSynchronizer(
        QnWorkbenchLayout* layout,
        QObject* parent = nullptr);

    virtual ~QnWorkbenchLayoutSynchronizer();

    QnWorkbenchLayout* layout() const
    {
        return m_layout;
    }

    /** Synchronizer associated with the given resource, or nullptr if none. */
    static QnWorkbenchLayoutSynchronizer* instance(const LayoutResourcePtr& resource);

public:
    void update();
    void reset();
    void submit();
    void submitPendingItems();
    void submitPendingItemsLater();

private:
    LayoutResourcePtr resource() const;

    void initialize();

    void at_resource_resourceChanged();
    void at_resource_itemAdded(
        const QnLayoutResourcePtr& resource, const nx::vms::common::LayoutItemData& itemData);
    void at_resource_itemRemoved(
        const QnLayoutResourcePtr& resource, const nx::vms::common::LayoutItemData& itemData);
    void at_resource_itemChanged(
        const QnLayoutResourcePtr& resource, const nx::vms::common::LayoutItemData& itemData);

    void at_layout_itemAdded(QnWorkbenchItem *item);
    void at_layout_itemRemoved(QnWorkbenchItem *item);

    void at_item_dataChanged(int role);
    void at_item_flagChanged(Qn::ItemFlag flag, bool value);

private:
    /** Associated workbench layout. */
    QnWorkbenchLayout* m_layout;

    /** Whether changes should be propagated from resource to layout. */
    bool m_update = false;

    /** Whether changes should be propagated from layout to resource. */
    bool m_submit = false;

    /** Whether queued submit is in progress. */
    bool m_submitPending = false;

    /** Queue of item UUIDs that are to be submitted in deferred mode.
     *
     * It is needed to prevent races when two concurrent changes to the same
     * item are synchronized there-and-back many times until finally converging. */
    QSet<nx::Uuid> m_pendingItems;
};
