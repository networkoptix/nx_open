#include "async_http_client_reply.h"

#include "asynchttpclient.h"

QnAsyncHttpClientReply::QnAsyncHttpClientReply(
    const nx_http::AsyncHttpClientPtr& client,
    QObject* parent)
    :
    QObject(parent),
    m_client(client)
{
    if (m_client)
    {
        connect(
            m_client.get(), &nx_http::AsyncHttpClient::done,
            this, &QnAsyncHttpClientReply::at_client_done,
            Qt::DirectConnection);
    }
}

nx_http::AsyncHttpClientPtr QnAsyncHttpClientReply::asyncHttpClient() const
{
    QnMutexLocker lock(&m_mutex);
    return m_client;
}

bool QnAsyncHttpClientReply::isFailed() const
{
    QnMutexLocker lock(&m_mutex);
    return m_failed;
}

nx::utils::Url QnAsyncHttpClientReply::url() const
{
    QnMutexLocker lock(&m_mutex);
    return m_url;
}

QByteArray QnAsyncHttpClientReply::contentType()
{
    QnMutexLocker lock(&m_mutex);
    return m_contentType;
}

QByteArray QnAsyncHttpClientReply::data()
{
    QnMutexLocker lock(&m_mutex);
    return m_data;
}

nx_http::Response QnAsyncHttpClientReply::response()
{
    QnMutexLocker lock(&m_mutex);
    return m_response;
}

void QnAsyncHttpClientReply::at_client_done(const nx_http::AsyncHttpClientPtr &client)
{
    QnMutexLocker lock(&m_mutex);

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
