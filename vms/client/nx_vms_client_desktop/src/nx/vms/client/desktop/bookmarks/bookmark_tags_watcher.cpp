// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_tags_watcher.h"

#include <chrono>

#include <QtCore/QTimer>

#include <camera/camera_bookmarks_manager.h>
#include <client/client_message_processor.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

namespace {

using namespace std::chrono_literals;

static constexpr auto kRefreshInterval = 30s;
static constexpr auto kTagsLimit = 20;

} // namespace

BookmarkTagsWatcher::BookmarkTagsWatcher(SystemContext* systemContext, QObject* parent):
    QObject(parent),
    SystemContextAware(systemContext),
    m_refreshTimer(new QTimer(this))
{
    m_refreshTimer->setInterval(kRefreshInterval);
    connect(m_refreshTimer, &QTimer::timeout, this, &BookmarkTagsWatcher::refresh);

    connect(qnClientMessageProcessor,
        &QnClientMessageProcessor::initialResourcesReceived,
        this,
        &BookmarkTagsWatcher::at_messageProcessor_connectionOpened);
    connect(qnClientMessageProcessor,
        &QnClientMessageProcessor::connectionClosed,
        this,
        &BookmarkTagsWatcher::at_messageProcessor_connectionClosed);
}

QnCameraBookmarkTagList BookmarkTagsWatcher::tags() const
{
    return m_tags;
}

void BookmarkTagsWatcher::refresh()
{
    auto systemContext = SystemContext::fromResource(currentServer());
    if (!NX_ASSERT(systemContext))
        return;

    auto bookmarkManager = systemContext->cameraBookmarksManager();
    bookmarkManager->getBookmarkTagsAsync(kTagsLimit,
        [this](bool success, int /*handle*/, const QnCameraBookmarkTagList& tags)
        {
            if (success)
                m_tags = tags;
        });
}

void BookmarkTagsWatcher::at_messageProcessor_connectionOpened()
{
    refresh();
    m_refreshTimer->start();
}

void BookmarkTagsWatcher::at_messageProcessor_connectionClosed()
{
    m_refreshTimer->stop();
    m_tags.clear();
}

} // namespace nx::vms::client::desktop
