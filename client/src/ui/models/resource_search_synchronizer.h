#ifndef QN_RESOURCE_SEARCH_SYNCHRONIZER_H
#define QN_RESOURCE_SEARCH_SYNCHRONIZER_H

#include <QObject>
#include <QHash>
#include <QModelIndex>
#include <core/resource/resource_fwd.h>

class QnWorkbenchItem;
class QnWorkbenchLayout;

class QnResourceCriterion;
class QnResourceSearchProxyModel;
class QnResourceSearchSynchronizerCriterion;

/**
 * This class performs bidirectional synchronization of 
 * <tt>QnWorkbenchLayout</tt> and <tt>QnResourceSearchProxyModel</tt> instances.
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
    void update();
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

    /** Whether the associated resource criterion should perform the checks. */
    bool m_criterionFunctional;

    /** Number of items in the model by resource. */
    QHash<QnResourcePtr, int> m_modelItemCountByResource;

    /** Result of last search operation. */
    QSet<QnResourcePtr> m_searchResult;

    /** Mapping from resource to an item that was added to layout as a result of search operation. */
    QHash<QnResourcePtr, QnWorkbenchItem *> m_layoutItemByResource;

    /** Mapping from an item that was added to layout as a result of search operation to resource. */
    QHash<QnWorkbenchItem *, QnResourcePtr> m_resourceByLayoutItem;

    /** Temporary object used for postponed updates. */
    QObject *m_updateListener;

    /** Resource criterion that is used to accept additional resources. */
    QScopedPointer<QnResourceCriterion> m_criterion;
};

#endif // QN_RESOURCE_SEARCH_SYNCHRONIZER_H
