// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webpage_resource_source.h"

#include <nx/vms/client/desktop/resource_views/entity_resource_tree/webpage_resource_index.h>
#include <core/resource/media_server_resource.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

WebpageResourceSource::WebpageResourceSource(
    const WebPageResourceIndex* webPagesIndex,
    bool includeProxiedWebPages)
    :
    base_type(),
    m_webPagesIndex(webPagesIndex),
    m_includeProxiedWebPages(includeProxiedWebPages)
{
    if (!NX_ASSERT(m_webPagesIndex))
        return;

    connect(m_webPagesIndex, &WebPageResourceIndex::webPageAddedToServer, this,
        [this](const QnResourcePtr& webPage, const QnResourcePtr& server)
        {
            if (server.isNull() || m_includeProxiedWebPages)
                emit resourceAdded(webPage);
        });

    connect(m_webPagesIndex, &WebPageResourceIndex::webPageRemovedFromServer, this,
        [this](const QnResourcePtr& webPage, const QnResourcePtr& server)
        {
            if (server.isNull() || m_includeProxiedWebPages)
                emit resourceRemoved(webPage);
        });
}

QVector<QnResourcePtr> WebpageResourceSource::getResources()
{
    return m_includeProxiedWebPages
        ? m_webPagesIndex->allWebPages()
        : m_webPagesIndex->webPagesOnServer(QnUuid());
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
