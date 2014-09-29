#include "resource_search_synchronizer.h"
#include <cassert>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/delete_later.h>
#include <utils/common/checked_cast.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_criterion.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_item.h>
#include "resource_search_proxy_model.h"
#include "resource_pool_model.h"

QnResourceSearchSynchronizer::QnResourceSearchSynchronizer(QObject *parent):
    QObject(parent),
    m_layout(NULL),
    m_model(NULL),
    m_submit(false),
    m_update(false),
    m_updatesEnabled(true),
    m_hasPendingUpdates(false),
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

    connect(m_layout,   SIGNAL(aboutToBeDestroyed()),                                   this, SLOT(at_layout_aboutToBeDestroyed()));
    connect(m_layout,   SIGNAL(itemAdded(QnWorkbenchItem *)),                           this, SLOT(at_layout_itemAdded(QnWorkbenchItem *)));
    connect(m_layout,   SIGNAL(itemRemoved(QnWorkbenchItem *)),                         this, SLOT(at_layout_itemRemoved(QnWorkbenchItem *)));

    connect(m_model,    SIGNAL(destroyed(QObject *)),                                   this, SLOT(at_model_destroyed()));
    connect(m_model,    SIGNAL(rowsInserted(const QModelIndex &, int, int)),            this, SLOT(at_model_rowsInserted(const QModelIndex &, int, int)));
    connect(m_model,    SIGNAL(rowsAboutToBeRemoved(const QModelIndex &, int, int)),    this, SLOT(at_model_rowsAboutToBeRemoved(const QModelIndex &, int, int)));
    connect(m_model,    SIGNAL(criteriaChanged()),                                      this, SLOT(at_model_criteriaChanged()));

    // TODO: #Elric we're assuming the model is empty, which may not be true.

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
    QN_SCOPED_VALUE_ROLLBACK(&m_submit, false);

    /* Create a set of items that match the search criteria. */
    QSet<QnResourcePtr> searchResult = m_modelItemCountByResource.keys().toSet();
    if(searchResult == m_searchResult)
        return;

    /* Add & remove items correspondingly. */
    QSet<QnResourcePtr> added = searchResult - m_searchResult;
    QSet<QnResourcePtr> removed = m_searchResult - searchResult;
    foreach(const QnResourcePtr &resource, added) {
        QnWorkbenchItem *item = new QnWorkbenchItem(resource->getUniqueId(), QnUuid::createUuid());

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

    if(*pos == 0) {
        m_modelItemCountByResource.erase(pos);
        updateLater();
    }
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

void QnResourceSearchSynchronizer::at_layout_itemAdded(QnWorkbenchItem *) {
    return; /* No layout-to-model propagation. */
}

void QnResourceSearchSynchronizer::at_layout_itemRemoved(QnWorkbenchItem *item) {
    QnResourcePtr resource = m_resourceByLayoutItem.value(item);
    if(resource.isNull()) 
        return;
    
    m_resourceByLayoutItem.remove(item);
    m_layoutItemByResource.remove(resource);
}

void QnResourceSearchSynchronizer::at_model_destroyed() {
    setModel(NULL);
}

void QnResourceSearchSynchronizer::at_model_rowsInserted(const QModelIndex &parent, int start, int end) {
    if(!m_update)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_submit, false);

    for (int row = start; row <= end; ++row) {
        if (!m_model->hasIndex(row, 0, parent))
            continue;
        const QModelIndex index = m_model->index(row, 0, parent);

        if (m_model->hasChildren(index))
            at_model_rowsInserted(index, 0, m_model->rowCount(index) - 1);

        QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
        if(resource.isNull())
            continue;

        // server widgets can be present in the tree but should not be present on the scene
        if (resource->hasFlags(Qn::server))
            continue;
        // layouts can be present in the tree but should not be present on the scene
        if (resource->hasFlags(Qn::layout))
            continue;

        addModelResource(resource);
    }
}

void QnResourceSearchSynchronizer::at_model_rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end) {
    if (!m_update)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_submit, false);

    for (int row = start; row <= end; ++row) {
        if (!m_model->hasIndex(row, 0, parent))
            continue;

        const QModelIndex index = m_model->index(row, 0, parent);

        if (m_model->hasChildren(index))
            at_model_rowsAboutToBeRemoved(index, 0, m_model->rowCount(index) - 1);

        QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
        if(resource.isNull())
            continue;

        removeModelResource(resource);
    }
}

void QnResourceSearchSynchronizer::at_model_criteriaChanged() {
    if(!m_update)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_submit, false);

    updateLater();
}

