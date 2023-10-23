// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_bookmark_tags_watcher.h"

#include <QtCore/QTimer>

#include <api/helpers/bookmark_request_data.h>
#include <camera/camera_bookmarks_manager.h>
#include <client/client_message_processor.h>
#include <client/client_module.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/desktop/system_context.h>

using namespace nx::vms::client::desktop;

QnWorkbenchBookmarkTagsWatcher::QnWorkbenchBookmarkTagsWatcher(QObject *parent)
    : QObject(parent)
    , m_refreshTimer(new QTimer(this))
{
    enum { TagsRefreshInterval = 30 * 1000 };

    m_refreshTimer->setInterval(TagsRefreshInterval);
    connect(m_refreshTimer, &QTimer::timeout, this, &QnWorkbenchBookmarkTagsWatcher::refresh);

    connect(qnClientMessageProcessor, &QnClientMessageProcessor::initialResourcesReceived,
            this, &QnWorkbenchBookmarkTagsWatcher::at_messageProcessor_connectionOpened);
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::connectionClosed,
            this, &QnWorkbenchBookmarkTagsWatcher::at_messageProcessor_connectionClosed);
}

QnCameraBookmarkTagList QnWorkbenchBookmarkTagsWatcher::tags() const {
    return m_tags;
}

void QnWorkbenchBookmarkTagsWatcher::refresh() {
    if (!currentServer())
        return;

    enum { TagsLimit = 20 };

    QnGetBookmarkTagsRequestData requestData(TagsLimit);
    requestData.format = Qn::SerializationFormat::ubjson;

    // FIXME: #sivanov Move watcher itself to the System Context.
    auto systemContext = SystemContext::fromResource(currentServer());
    auto bookmarkManager = systemContext->cameraBookmarksManager();
    bookmarkManager->getBookmarkTagsAsync(TagsLimit,
        [this](bool success, int /*handle*/, const QnCameraBookmarkTagList &tags)
        {
            this->at_connection_gotBookmarkTags(success, tags);
        });
}

void QnWorkbenchBookmarkTagsWatcher::at_connection_gotBookmarkTags(
        bool success, const QnCameraBookmarkTagList &tags)
{
    if (!success)
        return;

    m_tags = tags;

    emit tagsChanged(tags);
}

void QnWorkbenchBookmarkTagsWatcher::at_messageProcessor_connectionOpened() {
    refresh();
    m_refreshTimer->start();
}

void QnWorkbenchBookmarkTagsWatcher::at_messageProcessor_connectionClosed() {
    m_refreshTimer->stop();
    m_tags.clear();
    emit tagsChanged(m_tags);
}
