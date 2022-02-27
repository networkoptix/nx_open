// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_bookmark_tags_watcher.h"

#include <api/helpers/bookmark_request_data.h>

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <client/client_message_processor.h>
#include <client/client_module.h>
#include <camera/camera_bookmarks_manager.h>

#include <QtCore/QTimer>

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
    QnMediaServerResourcePtr server = commonModule()->currentServer();
    if (!server)
        return;

    enum { TagsLimit = 20 };

    QnGetBookmarkTagsRequestData requestData(TagsLimit);
    requestData.format = Qn::SerializationFormat::UbjsonFormat;

    auto bookmarkManager = commonModule()->instance<QnCameraBookmarksManager>();
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
