// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webpage_resource_source.h"

#include <core/resource/media_server_resource.h>
#include <core/resource/webpage_resource.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/webpage_resource_index.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

WebpageResourceSource::WebpageResourceSource(
    const WebPageResourceIndex* webPagesIndex,
    const std::optional<nx::vms::api::WebPageSubtype>& subtype,
    bool includeProxied)
    :
    base_type(),
    m_webPagesIndex(webPagesIndex),
    m_subtype(subtype),
    m_includeProxied(includeProxied)
{
    if (!NX_ASSERT(m_webPagesIndex))
        return;

    connect(m_webPagesIndex, &WebPageResourceIndex::webPageAddedToServer, this,
        [this](const QnResourcePtr& webPage, const QnResourcePtr& server)
        {
            if ((server.isNull() || m_includeProxied) && isSubtypeMatched(webPage))
                emit resourceAdded(webPage);
        });

    connect(m_webPagesIndex, &WebPageResourceIndex::webPageRemovedFromServer, this,
        [this](const QnResourcePtr& webPage, const QnResourcePtr& server)
        {
            if ((server.isNull() || m_includeProxied) && isSubtypeMatched(webPage))
                emit resourceRemoved(webPage);
        });

    connect(m_webPagesIndex, &WebPageResourceIndex::webPageSubtypeChanged, this,
        [this](const QnResourcePtr& webPage)
        {
            if (!m_subtype)
                return;

            if (isSubtypeMatched(webPage))
                emit resourceAdded(webPage);
            else
                emit resourceRemoved(webPage);
        });
}

QVector<QnResourcePtr> WebpageResourceSource::getResources()
{
    QVector<QnResourcePtr> webPages = m_includeProxied
        ? m_webPagesIndex->allWebPages()
        : m_webPagesIndex->webPagesOnServer(nx::Uuid());

    if (m_subtype)
    {
        webPages.removeIf(
            [this](const QnResourcePtr& webPage) { return !isSubtypeMatched(webPage); });
    }

    return webPages;
}

bool WebpageResourceSource::isSubtypeMatched(const QnResourcePtr& resource) const
{
    const auto webPage = resource.dynamicCast<QnWebPageResource>();
    if (!NX_ASSERT(webPage))
        return false;

    return !m_subtype || webPage->subtype() == m_subtype;
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
