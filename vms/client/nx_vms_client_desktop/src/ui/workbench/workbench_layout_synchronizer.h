// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QMetaType>
#include <QtCore/QSet>
#include <nx/utils/uuid.h>

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <utils/common/connective.h>

class QnWorkbench;
class QnWorkbenchItem;
class QnWorkbenchLayout;

/**
 * This class performs bidirectional synchronization of instances of
 * <tt>QnWorkbenchLayout</tt> and <tt>QnLayoutResource</tt>.
 */
class QnWorkbenchLayoutSynchronizer:
    public Connective<QObject>,
    public nx::vms::client::core::CommonModuleAware
{
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    QnWorkbenchLayoutSynchronizer(QnWorkbenchLayout *layout, const QnLayoutResourcePtr &resource, QObject *parent);

    virtual ~QnWorkbenchLayoutSynchronizer();

    QnWorkbenchLayout *layout() const {
        return m_layout;
    }

    const QnLayoutResourcePtr &resource() const {
        return m_resource;
    }

    /**
     * \returns                         Whether this synchronizer should delete itself
     *                                  once one of the associated objects is destroyed.
     */
    bool isAutoDeleting() const {
        return m_autoDeleting;
    }

    /**
     * \param autoDeleting              Whether this synchronizer should delete itself
     *                                  once one of the associated objects is destroyed.
     */
    void setAutoDeleting(bool autoDeleting);

    /**
     * \param layout                    Workbench layout.
     * \returns                         Synchronizer associated with the given layout, or nullptr if none.
     */
    static QnWorkbenchLayoutSynchronizer *instance(QnWorkbenchLayout *layout);

    /**
     * \param resource                  Layout resource.
     * \returns                         Synchronizer associated with the given resource, or nullptr if none.
     */
    static QnWorkbenchLayoutSynchronizer *instance(const QnLayoutResourcePtr &resource);

public slots:
    void update();
    void reset();
    void submit();
    void submitPendingItems();
    void submitPendingItemsLater();

protected:
    void clearLayout();
    void clearResource();

    void initialize();
    void deinitialize();

protected slots:
    void autoDelete();
    void autoDeleteLater();

    void at_resource_resourceChanged();
    void at_resource_nameChanged();
    void at_resource_itemAdded(const QnLayoutResourcePtr &resource, const QnLayoutItemData &itemData);
    void at_resource_itemRemoved(const QnLayoutResourcePtr &resource, const QnLayoutItemData &itemData);
    void at_resource_itemChanged(const QnLayoutResourcePtr &resource, const QnLayoutItemData &itemData);
    void at_resource_cellAspectRatioChanged();
    void at_resource_cellSpacingChanged();
    void at_resource_lockedChanged();

    void at_layout_itemAdded(QnWorkbenchItem *item);
    void at_layout_itemRemoved(QnWorkbenchItem *item);
    void at_layout_nameChanged();
    void at_layout_cellAspectRatioChanged();
    void at_layout_cellSpacingChanged();
    void at_layout_aboutToBeDestroyed();

    void at_item_dataChanged(int role);
    void at_item_flagChanged(Qn::ItemFlag flag, bool value);

private:
    /** Whether this synchronizer is functional. */
    bool m_running = false;

    /** Associated workbench layout. */
    QnWorkbenchLayout* m_layout;

    /** Associated layout resource. */
    QnLayoutResourcePtr m_resource;

    /** Whether changes should be propagated from resource to layout. */
    bool m_update = false;

    /** Whether changes should be propagated from layout to resource. */
    bool m_submit = false;

    /** Whether this layout synchronizer should delete itself once
     * one of the objects it manages is destroyed. */
    bool m_autoDeleting = false;

    /** Whether queued submit is in progress. */
    bool m_submitPending = false;

    /** Queue of item UUIDs that are to be submitted in deferred mode.
     *
     * It is needed to prevent races when two concurrent changes to the same
     * item are synchronized there-and-back many times until finally converging. */
    QSet<QnUuid> m_pendingItems;
};

Q_DECLARE_METATYPE(QnWorkbenchLayoutSynchronizer *);
