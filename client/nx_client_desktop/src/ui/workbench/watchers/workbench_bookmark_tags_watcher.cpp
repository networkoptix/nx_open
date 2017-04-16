#include "workbench_bookmark_tags_watcher.h"

#include <api/media_server_connection.h>
#include <api/helpers/bookmark_request_data.h>

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <client/client_message_processor.h>


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

    server->apiConnection()->getBookmarkTagsAsync(requestData,
        this, SLOT(at_connection_gotBookmarkTags(int,QnCameraBookmarkTagList,int)));
}

void QnWorkbenchBookmarkTagsWatcher::at_connection_gotBookmarkTags(
        int status, const QnCameraBookmarkTagList &tags, int handle)
{
    Q_UNUSED(handle);

    if (status != 0)
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
