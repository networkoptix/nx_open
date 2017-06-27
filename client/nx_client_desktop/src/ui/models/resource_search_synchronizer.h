#ifndef QN_RESOURCE_SEARCH_SYNCHRONIZER_H
#define QN_RESOURCE_SEARCH_SYNCHRONIZER_H

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QModelIndex>
#include <QtCore/QMetaType>

#include <core/resource/resource_fwd.h>

class QnWorkbenchItem;
class QnWorkbenchLayout;

class QnResourceCriterion;
class QnResourceSearchProxyModel;
class QnResourceSearchSynchronizerCriterion;

/**
 * This class performs bidirectional synchronization of
 * <tt>QnWorkbenchLayout</tt> and <tt>QnResourceSearchProxyModel</tt> instances.
 *
 * For all resources found, it adds new items to the layout. When the search
 * criteria changes, added items are replaced with the newly found ones.
 */
class QnResourceSearchSynchronizer: public QObject {
    Q_OBJECT;

public:
    QnResourceSearchSynchronizer(QObject *parent = NULL);

    virtual ~QnResourceSearchSynchronizer();

    void setModel(QnResourceSearchProxyModel *model);

    QnResourceSearchProxyModel *model() const {
        return m_model;
    }

    void setLayout(QnWorkbenchLayout *layout);

    QnWorkbenchLayout *layout() const {
        return m_layout;
    }

    void enableUpdates(bool enable = true);

    void disableUpdates(bool disable = true) {
        enableUpdates(!disable);
    }

public slots:
    /**
     * Forces layout update from the proxy model.
     */
    void update();

    /**
     * Performs delayed update of the layout from the proxy model.
     *
     * Layout will be update only once even if this function was
     * called several times.
     */
    void updateLater();

protected:
    void start();
    void stop();

    void addModelResource(const QnResourcePtr &resource);
    void removeModelResource(const QnResourcePtr &resource);

protected slots:
    void at_layout_aboutToBeDestroyed();
    void at_layout_itemAdded(QnWorkbenchItem *item);
    void at_layout_itemRemoved(QnWorkbenchItem *item);

    void at_model_destroyed();
    void at_model_rowsInserted(const QModelIndex &parent, int start, int end);
    void at_model_rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end);
    void at_model_criteriaChanged();

private:
    friend class QnResourceSearchSynchronizerCriterion;

    /** Associated layout. */
    QnWorkbenchLayout *m_layout;

    /** Associated model. */
    QnResourceSearchProxyModel *m_model;

    /** Whether changes should be propagated from layout to model. */
    bool m_submit;

    /** Whether changes should be propagated from model to layout. */
    bool m_update;

    /** Whether updates are enabled. */
    bool m_updatesEnabled;

    /** Whether there were update requests while updates were disabled. */
    bool m_hasPendingUpdates;

    /** Number of items in the search model by resource. */
    QHash<QnResourcePtr, int> m_modelItemCountByResource;

    /** Result of the last search operation. */
    QSet<QnResourcePtr> m_searchResult;

    /** Mapping from resource to an item that was added to layout as a result of a search operation. */
    QHash<QnResourcePtr, QnWorkbenchItem *> m_layoutItemByResource;

    /** Mapping from an item that was added to layout as a result of search operation to resource. */
    QHash<QnWorkbenchItem *, QnResourcePtr> m_resourceByLayoutItem;

    /** Temporary object used for postponed updates. */
    QObject *m_updateListener;
};

Q_DECLARE_METATYPE(QnResourceSearchSynchronizer *);

#endif // QN_RESOURCE_SEARCH_SYNCHRONIZER_H
