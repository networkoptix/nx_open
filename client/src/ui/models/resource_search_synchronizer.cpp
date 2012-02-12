#include "resource_search_synchronizer.h"
#include <cassert>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/delete_later.h>
#include <utils/common/checked_cast.h>
#include <core/resource/resource.h>
#include <core/resourcemanagment/resource_criterion.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_item.h>
#include "resource_search_proxy_model.h"
#include "resource_model.h"

Q_DECLARE_METATYPE(QWeakPointer<QnResourceSearchSynchronizer>);

class QnResourceSearchSynchronizerCriterion: public QObject, public QnResourceCriterion {
public:
    QnResourceSearchSynchronizerCriterion(QnResourceSearchSynchronizer *synchronizer):
        QObject(NULL),
        QnResourceCriterion(&QnResourceSearchSynchronizerCriterion::check, QVariant())
    {
        setTargetValue(QVariant::fromValue<QWeakPointer<QnResourceSearchSynchronizer> >(QWeakPointer<QnResourceSearchSynchronizer>(synchronizer)));
    }

    static QnResourceCriterion::Operation check(const QnResourcePtr &resource, const QVariant &targetValue) {
        QWeakPointer<QnResourceSearchSynchronizer> synchronizerPointer = targetValue.value<QWeakPointer<QnResourceSearchSynchronizer> >();
        if(synchronizerPointer.isNull())
            return QnResourceCriterion::NEXT;

        QnResourceSearchSynchronizer *synchronizer = synchronizerPointer.data();
        if(!synchronizer->m_submit) /* That would mean that synchronizer is stopped. */
            return QnResourceCriterion::NEXT; 

        if(!synchronizer->m_criterionFunctional)
            return QnResourceCriterion::NEXT; 

        if(synchronizer->m_layout->items(resource->getUniqueId()).size() > 0)
            return QnResourceCriterion::ACCEPT;

        return QnResourceCriterion::NEXT;
    }
};

QnResourceSearchSynchronizer::QnResourceSearchSynchronizer(QObject *parent):
    QObject(parent),
    m_layout(NULL),
    m_model(NULL),
    m_submit(false),
    m_update(false),
    m_updatesEnabled(true),
    m_hasPendingUpdates(false),
    m_criterionFunctional(true),
    m_updateListener(NULL)
{}

QnResourceSearchSynchronizer::~QnResourceSearchSynchronizer() {
    setModel(NULL);
    setLayout(NULL);
}

void QnResourceSearchSynchronizer::setModel(QnResourceSearchProxyModel *model) {
    if(m_model == model)
        return;

    if(m_model != NULL && m_layout != NULL)
        stop();

    m_model = model;

    if(m_model != NULL && m_layout != NULL)
        start();
}

void QnResourceSearchSynchronizer::setLayout(QnWorkbenchLayout *layout) {
    if(m_layout == layout)
        return;

    if(m_model != NULL && m_layout != NULL)
        stop();

    m_layout = layout;

    if(m_model != NULL && m_layout != NULL)
        start();
}

void QnResourceSearchSynchronizer::start() {
    assert(m_model != NULL && m_layout != NULL);

    m_criterion = new QnResourceSearchSynchronizerCriterion(this);

    connect(m_layout,   SIGNAL(aboutToBeDestroyed()),                                   this, SLOT(at_layout_aboutToBeDestroyed()));
    connect(m_layout,   SIGNAL(itemAdded(QnWorkbenchItem *)),                           this, SLOT(at_layout_itemAdded(QnWorkbenchItem *)));
    connect(m_layout,   SIGNAL(itemRemoved(QnWorkbenchItem *)),                         this, SLOT(at_layout_itemRemoved(QnWorkbenchItem *)));

    connect(m_model,    SIGNAL(destroyed(QObject *)),                                   this, SLOT(at_model_destroyed()));
    connect(m_model,    SIGNAL(rowsInserted(const QModelIndex &, int, int)),            this, SLOT(at_model_rowsInserted(const QModelIndex &, int, int)));
    connect(m_model,    SIGNAL(rowsAboutToBeRemoved(const QModelIndex &, int, int)),    this, SLOT(at_model_rowsAboutToBeRemoved(const QModelIndex &, int, int)));
    connect(m_model,    SIGNAL(criteriaChanged()),                                      this, SLOT(at_model_criteriaChanged()));

    // TODO: we're assuming the model is empty.

    m_submit = m_update = true;
    m_hasPendingUpdates = false;
}


void QnResourceSearchSynchronizer::stop() {
    assert(m_model != NULL && m_layout != NULL);

    /* Don't modify model or layout, just clear internal data structures. */
    m_modelItemCountByResource.clear();
    m_searchResult.clear();
    m_layoutItemByResource.clear();
    m_resourceByLayoutItem.clear();
    if(m_updateListener != NULL) {
        disconnect(m_updateListener, NULL, this, NULL);
        m_updateListener = NULL;
    }

    disconnect(m_layout, NULL, this, NULL);
    disconnect(m_model, NULL, this, NULL);

    /* Model may be already destroyed, together with our criterion... */
    if(m_criterion.data() != NULL) {
        m_model->removeCriterion(m_criterion.data());
        delete m_criterion.data();
    }

    m_submit = m_update = false;
}

void QnResourceSearchSynchronizer::update() {
    if(m_updateListener != NULL) {
        disconnect(m_updateListener, NULL, this, NULL);
        m_updateListener = NULL;
    }

    if(!m_update)
        return;

    if(!m_updatesEnabled) {
        m_hasPendingUpdates = true;
        return;
    }

    m_hasPendingUpdates = false;
    QnScopedValueRollback<bool> guard(&m_submit, false);

    /* Create a set of items that match the actual criteria. */
    QSet<QnResourcePtr> searchResult;
    {
        QnScopedValueRollback<bool> guard(&m_criterionFunctional, false);
        const QnResourceCriterionGroup *criteria = m_model->criteria();
        for(QHash<QnResourcePtr, int>::const_iterator pos = m_modelItemCountByResource.begin(), end = m_modelItemCountByResource.end(); pos != end; pos++)
            if(criteria->check(pos.key()) == QnResourceCriterion::ACCEPT)
                searchResult.insert(pos.key());
        if(m_searchResult == searchResult)
            return;
    }

    /* Add & remove items correspondingly. */
    QSet<QnResourcePtr> added = searchResult - m_searchResult;
    QSet<QnResourcePtr> removed = m_searchResult - searchResult;
    foreach(const QnResourcePtr &resource, added) {
        QnWorkbenchItem *item = new QnWorkbenchItem(resource->getUniqueId(), QUuid::createUuid());

        m_resourceByLayoutItem[item] = resource;
        m_layoutItemByResource[resource] = item;

        m_layout->addItem(item);
        item->adjustGeometry();
    }
    foreach(const QnResourcePtr &resource, removed) {
        QnWorkbenchItem *item = m_layoutItemByResource.value(resource);
        if(item == NULL)
            continue; /* Was closed by the user. */
        
        m_resourceByLayoutItem.remove(item);
        m_layoutItemByResource.remove(resource);

        delete item;
    }

    /* Store current search result for further use. */
    m_searchResult = searchResult;
}

void QnResourceSearchSynchronizer::updateLater() {
    if(!m_update)
        return;

    if(!m_updatesEnabled) {
        m_hasPendingUpdates = true;
        return;
    }

    if(m_updateListener != NULL)
        return; /* Already waiting for an update. */

    m_updateListener = new QObject(this);
    connect(m_updateListener, SIGNAL(destroyed()), this, SLOT(update()));
    qnDeleteLater(m_updateListener);
}

void QnResourceSearchSynchronizer::addModelResource(const QnResourcePtr &resource) {
    int &count = m_modelItemCountByResource[resource];
    if(count == 0)
        updateLater();
    count++;
}

void QnResourceSearchSynchronizer::removeModelResource(const QnResourcePtr &resource) {
    QHash<QnResourcePtr, int>::iterator pos = m_modelItemCountByResource.find(resource);
    if(pos == m_modelItemCountByResource.end())
        return; /* Something went wrong... */

    (*pos)--;

    if(*pos == 0)
        updateLater();
}

void QnResourceSearchSynchronizer::enableUpdates(bool enable) {
    if(m_updatesEnabled == enable)
        return;

    m_updatesEnabled = enable;
    
    if(m_updatesEnabled && m_hasPendingUpdates)
        updateLater();
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnResourceSearchSynchronizer::at_layout_aboutToBeDestroyed() {
    setLayout(NULL);
}

void QnResourceSearchSynchronizer::at_layout_itemAdded(QnWorkbenchItem *item) {
    if(!m_submit)
        return;

    QnScopedValueRollback<bool> guard(&m_update, false);

    if(item->layout()->items(item->resourceUid()).size() == 1)
        m_model->invalidateFilterLater();
}

void QnResourceSearchSynchronizer::at_layout_itemRemoved(QnWorkbenchItem *item) {
    if(!m_submit)
        return;

    QnScopedValueRollback<bool> guard(&m_update, false);
    
    QnResourcePtr resource = m_resourceByLayoutItem.value(item);
    if(!resource.isNull()) {
        m_resourceByLayoutItem.remove(item);
        m_layoutItemByResource.remove(resource);
    } else {
        QnWorkbenchLayout *layout = checked_cast<QnWorkbenchLayout *>(sender());
        if(layout->items(item->resourceUid()).size() == 0)
            m_model->invalidateFilterLater();
    }
}

void QnResourceSearchSynchronizer::at_model_destroyed() {
    setModel(NULL);
}

void QnResourceSearchSynchronizer::at_model_rowsInserted(const QModelIndex &parent, int start, int end) {
    if(!m_update)
        return;

    QnScopedValueRollback<bool> guard(&m_submit, false);

    for (int row = start; row <= end; ++row) {
        const QModelIndex index = parent.child(row, 0);
        QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
        if(resource.isNull())
            continue;

        addModelResource(resource);
    }
}

void QnResourceSearchSynchronizer::at_model_rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end) {
    if (!m_update)
        return;

    QnScopedValueRollback<bool> guard(&m_submit, false);

    for (int row = start; row <= end; ++row) {
        const QModelIndex index = parent.child(row, 0);
        QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
        if(resource.isNull())
            continue;

        removeModelResource(resource);
    }
}

void QnResourceSearchSynchronizer::at_model_criteriaChanged() {
    if(!m_update)
        return;

    QnScopedValueRollback<bool> guard(&m_submit, false);

    updateLater();
}