#pragma once

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QModelIndex>
#include <QtCore/QMetaType>

#include <core/resource/resource_fwd.h>

class QnWorkbenchItem;
class QnWorkbenchLayout;

class QnResourceSearchProxyModel;

/**
 * This class performs synchronization of
 * <tt>QnWorkbenchLayout</tt> and <tt>QnResourceSearchProxyModel</tt> instances.
 *
 * For all resources found, it adds new items to the layout. When the search
 * criteria changes, added items are replaced with the newly found ones.
 */
class QnResourceSearchSynchronizer: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnResourceSearchSynchronizer(QObject* parent = nullptr);
    virtual ~QnResourceSearchSynchronizer() override;

    QnResourceSearchProxyModel* model() const;
    void setModel(QnResourceSearchProxyModel* model);

    QnWorkbenchLayout* layout() const;
    void setLayout(QnWorkbenchLayout* layout);


    void enableUpdates(bool enable = true);
    void disableUpdates(bool disable = true)
    {
        enableUpdates(!disable);
    }

public:
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

private:
    bool isValid() const;

    void start();
    void stop();

    void addModelResource(const QnResourcePtr& resource);
    void removeModelResource(const QnResourcePtr& resource);

    void at_layout_aboutToBeDestroyed();
    void at_layout_itemRemoved(QnWorkbenchItem* item);

    void at_model_destroyed();
    void at_model_rowsInserted(const QModelIndex& parent, int start, int end);
    void at_model_rowsAboutToBeRemoved(const QModelIndex& parent, int start, int end);
    void at_model_criteriaChanged();

private:
    /** Associated layout. */
    QnWorkbenchLayout* m_layout = nullptr;

    /** Associated model. */
    QnResourceSearchProxyModel* m_model = nullptr;

    /** Whether changes should be propagated from model to layout. */
    bool m_update = false;

    /** Whether updates are enabled. */
    bool m_updatesEnabled = true;

    /** Whether there were update requests while updates were disabled. */
    bool m_hasPendingUpdates = false;

    /** Number of items in the search model by resource. */
    QHash<QnResourcePtr, int> m_modelItemCountByResource;

    /** Result of the last search operation. */
    QSet<QnResourcePtr> m_searchResult;

    /** Mapping from resource to an item that was added to layout as a result of a search operation. */
    QHash<QnResourcePtr, QnWorkbenchItem*> m_layoutItemByResource;

    /** Mapping from an item that was added to layout as a result of search operation to resource. */
    QHash<QnWorkbenchItem*, QnResourcePtr> m_resourceByLayoutItem;

    /** Temporary object used for postponed updates. */
    QObject* m_updateListener = nullptr;
};

Q_DECLARE_METATYPE(QnResourceSearchSynchronizer *);
