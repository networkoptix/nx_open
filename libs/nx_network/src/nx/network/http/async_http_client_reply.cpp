// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "async_http_client_reply.h"

QnAsyncHttpClientReply::QnAsyncHttpClientReply(
    const nx::network::http::AsyncHttpClientPtr& client,
    QObject* parent)
    :
    QObject(parent),
    m_client(client)
{
    if (m_client)
    {
        connect(
            m_client.get(), &nx::network::http::AsyncHttpClient::done,
            this, &QnAsyncHttpClientReply::at_client_done,
            Qt::DirectConnection);
    }
}

nx::network::http::AsyncHttpClientPtr QnAsyncHttpClientReply::asyncHttpClient() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_client;
}

bool QnAsyncHttpClientReply::isFailed() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_failed;
}

nx::utils::Url QnAsyncHttpClientReply::url() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_url;
}

std::string QnAsyncHttpClientReply::contentType()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_contentType;
}

nx::Buffer QnAsyncHttpClientReply::data()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_data;
}

nx::network::http::Response QnAsyncHttpClientReply::response()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_response;
}

void QnAsyncHttpClientReply::at_client_done(const nx::network::http::AsyncHttpClientPtr &client)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (client != m_client)
        return;

    m_url = client->url();

    if (!(m_failed = client->failed()))
    {
        m_contentType = client->contentType();
        m_data = client->fetchMessageBodyBuffer();
        m_response = *client->response();
    }

    lock.unlock();

    client->pleaseStopSync();
    emit finished(this);
}
