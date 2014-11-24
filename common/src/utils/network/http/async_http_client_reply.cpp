#include "async_http_client_reply.h"

#include "asynchttpclient.h"

QnAsyncHttpClientReply::QnAsyncHttpClientReply(const nx_http::AsyncHttpClientPtr &client, QObject *parent) :
    QObject(parent),
    m_client(client)
{
    if (m_client)
        connect(m_client.get(), &nx_http::AsyncHttpClient::done, this, &QnAsyncHttpClientReply::at_client_done, Qt::DirectConnection);
}

nx_http::AsyncHttpClientPtr QnAsyncHttpClientReply::asyncHttpClient() const {
    QMutexLocker lock(&m_mutex);
    return m_client;
}

bool QnAsyncHttpClientReply::isFailed() const {
    QMutexLocker lock(&m_mutex);
    return m_failed;
}

QUrl QnAsyncHttpClientReply::url() const {
    QMutexLocker lock(&m_mutex);
    return m_url;
}

QByteArray QnAsyncHttpClientReply::contentType() {
    QMutexLocker lock(&m_mutex);
    return m_contentType;
}

QByteArray QnAsyncHttpClientReply::data() {
    QMutexLocker lock(&m_mutex);
    return m_data;
}

nx_http::Response QnAsyncHttpClientReply::response() {
    QMutexLocker lock(&m_mutex);
    return m_response;
}

void QnAsyncHttpClientReply::at_client_done(const nx_http::AsyncHttpClientPtr &client) {
    QMutexLocker lock(&m_mutex);

    if (client != m_client)
        return;

    client->terminate();

    m_url = client->url();

    if (!(m_failed = client->failed())) {
        m_contentType = client->contentType();
        m_data = client->fetchMessageBodyBuffer();
        m_response = *client->response();
    }

    lock.unlock();

    emit finished(this);
}
