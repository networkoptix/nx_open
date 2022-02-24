// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "proxied_web_resource_source.h"

#include <nx/vms/client/desktop/resource_views/entity_resource_tree/webpage_resource_index.h>
#include <core/resource/media_server_resource.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

ProxiedWebResourceSource::ProxiedWebResourceSource(
    const WebPageResourceIndex* webPagesIndex,
    const QnMediaServerResourcePtr& server)
    :
    base_type(),
    m_webPagesIndex(webPagesIndex),
    m_server(server)
{
    if (!NX_ASSERT(m_webPagesIndex))
        return;

    if (!m_server)
    {
        connect(m_webPagesIndex, &WebPageResourceIndex::webPageAddedToServer, this,
            [this](const QnResourcePtr& webPage, const QnResourcePtr& server)
            {
                if (server)
                    emit resourceAdded(webPage);
            });

        connect(m_webPagesIndex, &WebPageResourceIndex::webPageRemovedFromServer, this,
            [this](const QnResourcePtr& webPage, const QnResourcePtr& server)
            {
                if (server)
                    emit resourceRemoved(webPage);
            });
    }
    else
    {
        connect(m_webPagesIndex, &WebPageResourceIndex::webPageAddedToServer, this,
            [this](const QnResourcePtr& webPage, const QnResourcePtr& server)
            {
                if (server == m_server)
                    emit resourceAdded(webPage);
            });

        connect(m_webPagesIndex, &WebPageResourceIndex::webPageRemovedFromServer, this,
            [this](const QnResourcePtr& webPage, const QnResourcePtr& server)
            {
                if (server == m_server)
                    emit resourceRemoved(webPage);
            });
    }
}

QVector<QnResourcePtr> ProxiedWebResourceSource::getResources()
{
    if (m_server)
        return m_webPagesIndex->webPagesOnServer(m_server.staticCast<QnResource>()->getId());

    return m_webPagesIndex->allProxiedWebResources();
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
