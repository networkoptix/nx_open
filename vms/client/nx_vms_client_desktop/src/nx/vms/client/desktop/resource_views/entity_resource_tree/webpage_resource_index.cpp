// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webpage_resource_index.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/webpage_resource.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

WebPageResourceIndex::WebPageResourceIndex(const QnResourcePool* resourcePool):
    base_type(),
    m_resourcePool(resourcePool)
{
    if (!(NX_ASSERT(resourcePool)))
        return;

    blockSignals(true);
    indexAllWebPages();
    blockSignals(false);

    connect(m_resourcePool, &QnResourcePool::resourceAdded,
        this, &WebPageResourceIndex::onResourceAdded);

    connect(m_resourcePool, &QnResourcePool::resourceRemoved,
        this, &WebPageResourceIndex::onResourceRemoved);
}

QVector<QnResourcePtr> WebPageResourceIndex::allProxiedWebResources() const
{
    QVector<QnResourcePtr> result;

    for (auto i = m_webPagesByServer.cbegin(); i != m_webPagesByServer.cend(); ++i)
    {
        if (!i.key().isNull())
            std::copy(i.value().cbegin(), i.value().cend(), std::back_inserter(result));
    }

    return result;
}

QVector<QnResourcePtr> WebPageResourceIndex::webPagesOnServer(const nx::Uuid& serverId) const
{
    const auto& webPagesSet = m_webPagesByServer.value(serverId);
    return QVector<QnResourcePtr>(webPagesSet.cbegin(), webPagesSet.cend());
}

QVector<QnResourcePtr> WebPageResourceIndex::allWebPages() const
{
    const auto webPages = m_resourcePool->getResources<QnWebPageResource>();
    return QVector<QnResourcePtr>(webPages.cbegin(), webPages.cend());
}

void WebPageResourceIndex::indexAllWebPages()
{
    const QnResourceList webPages =
        m_resourcePool->getResourcesWithFlag(Qn::web_page);

    for (const auto& resource: webPages)
    {
        if (const auto webPage = resource.dynamicCast<QnWebPageResource>())
            indexWebPage(webPage);
    }
}

void WebPageResourceIndex::onResourceAdded(const QnResourcePtr& resource)
{
    if (const auto webPage = resource.dynamicCast<QnWebPageResource>())
        indexWebPage(webPage);
}

void WebPageResourceIndex::onResourceRemoved(const QnResourcePtr& resource)
{
    if (const auto webPage = resource.dynamicCast<QnWebPageResource>())
    {
        m_webPagesByServer[webPage->getParentId()].remove(resource);
        emit webPageRemoved(webPage);
        emit webPageRemovedFromServer(webPage, webPage->getParentResource());
    }
}

void WebPageResourceIndex::onWebPageParentIdChanged(
    const QnResourcePtr& resource,
    const nx::Uuid& previousParentServerId)
{
    const auto webPage = resource.staticCast<QnWebPageResource>();

    const nx::Uuid parentServerId = webPage->getParentId();
    const auto previousParentServer = m_resourcePool->getResourceById(previousParentServerId);

    m_webPagesByServer[previousParentServerId].remove(webPage);
    emit webPageRemovedFromServer(webPage, previousParentServer);

    m_webPagesByServer[parentServerId].insert(webPage);
    emit webPageAddedToServer(webPage, webPage->getParentResource());
}

void WebPageResourceIndex::indexWebPage(const QnWebPageResourcePtr& webPage)
{
    m_webPagesByServer[webPage->getParentId()].insert(webPage);

    connect(webPage.get(), &QnResource::parentIdChanged,
        this, &WebPageResourceIndex::onWebPageParentIdChanged);

    connect(webPage.get(), &QnWebPageResource::subtypeChanged,
        this, &WebPageResourceIndex::webPageSubtypeChanged);

    emit webPageAdded(webPage);
    emit webPageAddedToServer(webPage, webPage->getParentResource());
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
