#ifndef QN_WORKBENCH_LAYOUT_SYNCHRONIZER_H
#define QN_WORKBENCH_LAYOUT_SYNCHRONIZER_H

#include <QtCore/QObject>
#include <QtCore/QMetaType>
#include <QtCore/QSet>
#include <utils/common/uuid.h>

#include <utils/common/connective.h>

#include <core/resource/resource_fwd.h>

#include <client/client_globals.h>

class QnWorkbench;
class QnWorkbenchItem;
class QnWorkbenchLayout;
class QnLayoutItemData;

/**
 * This class performs bidirectional synchronization of instances of 
 * <tt>QnWorkbenchLayout</tt> and <tt>QnLayoutResource</tt>.
 */
class QnWorkbenchLayoutSynchronizer: public Connective<QObject> {
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
     * \returns                         Synchronizer associated with the given layout, or NULL if none.
     */
    static QnWorkbenchLayoutSynchronizer *instance(QnWorkbenchLayout *layout);

    /**
     * \param resource                  Layout resource.
     * \returns                         Synchronizer associated with the given resource, or NULL if none.
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
    bool m_running;

    /** Associated workbench layout. */
    QnWorkbenchLayout *m_layout;

    /** Associated layout resource. */
    QnLayoutResourcePtr m_resource;

    /** Whether changes should be propagated from resource to layout. */
    bool m_update;

    /** Whether changes should be propagated from layout to resource. */
    bool m_submit;

    /** Whether this layout synchronizer should delete itself once 
     * one of the objects it manages is destroyed. */
    bool m_autoDeleting;

    /** Whether queued submit is in progress. */
    bool m_submitPending;

    /** Queue of item UUIDs that are to be submitted in deferred mode. 
     *
     * It is needed to prevent races when two concurrent changes to the same
     * item are synchronized there-and-back many times until finally converging. */
    QSet<QnUuid> m_pendingItems;
};

Q_DECLARE_METATYPE(QnWorkbenchLayoutSynchronizer *);

#endif // QN_WORKBENCH_LAYOUT_SYNCHRONIZER_H
