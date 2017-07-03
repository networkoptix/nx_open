#include "resource_search_synchronizer.h"

#include <core/resource_access/resource_access_filter.h>

#include <core/resource/resource.h>

#include <ui/models/resource_search_proxy_model.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_item.h>

#include <utils/common/scoped_value_rollback.h>
#include <utils/common/delete_later.h>

QnResourceSearchSynchronizer::QnResourceSearchSynchronizer(QObject* parent):
    base_type(parent)
{
}

QnResourceSearchSynchronizer::~QnResourceSearchSynchronizer()
{
    setModel(nullptr);
    setLayout(nullptr);
}

QnResourceSearchProxyModel* QnResourceSearchSynchronizer::model() const
{
    return m_model;
}

void QnResourceSearchSynchronizer::setModel(QnResourceSearchProxyModel* model)
{
    if (m_model == model)
        return;

    if (isValid())
        stop();

    m_model = model;

    if (isValid())
        start();
}

QnWorkbenchLayout* QnResourceSearchSynchronizer::layout() const
{
    return m_layout;
}

void QnResourceSearchSynchronizer::setLayout(QnWorkbenchLayout* layout)
{
    if (m_layout == layout)
        return;

    if (isValid())
        stop();

    m_layout = layout;

    if (isValid())
        start();
}

bool QnResourceSearchSynchronizer::isValid() const
{
    return m_model && m_layout;
}

void QnResourceSearchSynchronizer::start()
{
    NX_ASSERT(isValid());

    connect(m_layout, &QnWorkbenchLayout::aboutToBeDestroyed, this,
        &QnResourceSearchSynchronizer::at_layout_aboutToBeDestroyed);
    connect(m_layout, &QnWorkbenchLayout::itemRemoved, this,
        &QnResourceSearchSynchronizer::at_layout_itemRemoved);

    connect(m_model, &QnResourceSearchProxyModel::destroyed, this,
        &QnResourceSearchSynchronizer::at_model_destroyed);
    connect(m_model, &QnResourceSearchProxyModel::rowsInserted, this,
        &QnResourceSearchSynchronizer::at_model_rowsInserted);
    connect(m_model, &QnResourceSearchProxyModel::rowsAboutToBeRemoved, this,
        &QnResourceSearchSynchronizer::at_model_rowsAboutToBeRemoved);
    connect(m_model, &QnResourceSearchProxyModel::criteriaChanged, this,
        &QnResourceSearchSynchronizer::at_model_criteriaChanged);

    // TODO: #Elric we're assuming the model is empty, which may not be true.

    m_update = true;
    m_hasPendingUpdates = false;
}

void QnResourceSearchSynchronizer::stop()
{
    NX_ASSERT(isValid());

    /* Don't modify model or layout, just clear internal data structures. */
    m_modelItemCountByResource.clear();
    m_searchResult.clear();
    m_layoutItemByResource.clear();
    m_resourceByLayoutItem.clear();
    if (m_updateListener)
    {
        m_updateListener->disconnect(this);
        m_updateListener = nullptr;
    }

    m_layout->disconnect(this);
    m_model->disconnect(this);

    m_update = false;
}

void QnResourceSearchSynchronizer::update()
{
    if (m_updateListener)
    {
        m_updateListener->disconnect(this);
        m_updateListener = nullptr;
    }

    if (!m_update)
        return;

    if (!m_updatesEnabled)
    {
        m_hasPendingUpdates = true;
        return;
    }

    m_hasPendingUpdates = false;

    /* Create a set of items that match the search criteria. */
    const auto searchResult = m_modelItemCountByResource.keys().toSet();
    if (searchResult == m_searchResult)
        return;

    /* Add & remove items correspondingly. */
    QSet<QnResourcePtr> added = searchResult - m_searchResult;
    QSet<QnResourcePtr> removed = m_searchResult - searchResult;
    for (const auto& resource : added)
    {
        NX_ASSERT(resource);
        if (!resource)
            continue;

        auto item = new QnWorkbenchItem(resource, QnUuid::createUuid());

        m_resourceByLayoutItem[item] = resource;
        m_layoutItemByResource[resource] = item;

        m_layout->addItem(item);
        item->adjustGeometry();
    }

    for (const auto& resource: removed)
    {
        auto item = m_layoutItemByResource.value(resource);
        if (item == NULL)
            continue; /* Was closed by the user. */

        m_resourceByLayoutItem.remove(item);
        m_layoutItemByResource.remove(resource);

        delete item;
    }

    /* Store current search result for further use. */
    m_searchResult = searchResult;
}

void QnResourceSearchSynchronizer::updateLater()
{
    if (!m_update)
        return;

    if (!m_updatesEnabled)
    {
        m_hasPendingUpdates = true;
        return;
    }

    if (m_updateListener != NULL)
        return; /* Already waiting for an update. */

    m_updateListener = new QObject(this);
    connect(m_updateListener, &QObject::destroyed, this, &QnResourceSearchSynchronizer::update);
    qnDeleteLater(m_updateListener);
}

void QnResourceSearchSynchronizer::addModelResource(const QnResourcePtr& resource)
{
    int &count = m_modelItemCountByResource[resource];
    if (count == 0)
        updateLater();
    count++;
}

void QnResourceSearchSynchronizer::removeModelResource(const QnResourcePtr& resource)
{
    QHash<QnResourcePtr, int>::iterator pos = m_modelItemCountByResource.find(resource);
    if (pos == m_modelItemCountByResource.end())
        return; /* Something went wrong... */

    (*pos)--;

    if (*pos == 0)
    {
        m_modelItemCountByResource.erase(pos);
        updateLater();
    }
}

void QnResourceSearchSynchronizer::enableUpdates(bool enable)
{
    if (m_updatesEnabled == enable)
        return;

    m_updatesEnabled = enable;

    if (m_updatesEnabled && m_hasPendingUpdates)
        updateLater();
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnResourceSearchSynchronizer::at_layout_aboutToBeDestroyed()
{
    setLayout(nullptr);
}

void QnResourceSearchSynchronizer::at_layout_itemRemoved(QnWorkbenchItem* item)
{
    auto resource = m_resourceByLayoutItem.value(item);
    if (!resource)
        return;

    m_resourceByLayoutItem.remove(item);
    m_layoutItemByResource.remove(resource);
}

void QnResourceSearchSynchronizer::at_model_destroyed()
{
    setModel(nullptr);
}

void QnResourceSearchSynchronizer::at_model_rowsInserted(const QModelIndex& parent, int start, int end)
{
    if (!m_update)
        return;

    for (int row = start; row <= end; ++row)
    {
        if (!m_model->hasIndex(row, 0, parent))
            continue;
        const QModelIndex index = m_model->index(row, 0, parent);

        if (m_model->hasChildren(index))
            at_model_rowsInserted(index, 0, m_model->rowCount(index) - 1);

        QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
        if (resource.isNull())
            continue;

        // Server widgets can be present in the tree but should not be present on the scene.
        if (resource->hasFlags(Qn::server))
            continue;

        // Web pages are too slow and buggy to be opened.
        if (resource->hasFlags(Qn::web_page))
            continue;

        if (!QnResourceAccessFilter::isOpenableInLayout(resource))
            continue;

        addModelResource(resource);
    }
}

void QnResourceSearchSynchronizer::at_model_rowsAboutToBeRemoved(const QModelIndex& parent, int start, int end)
{
    if (!m_update)
        return;

    for (int row = start; row <= end; ++row)
    {
        if (!m_model->hasIndex(row, 0, parent))
            continue;

        const QModelIndex index = m_model->index(row, 0, parent);

        if (m_model->hasChildren(index))
            at_model_rowsAboutToBeRemoved(index, 0, m_model->rowCount(index) - 1);

        QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
        if (resource.isNull())
            continue;

        removeModelResource(resource);
    }
}

void QnResourceSearchSynchronizer::at_model_criteriaChanged()
{
    if (!m_update)
        return;

    updateLater();
}
