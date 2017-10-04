#ifndef ASYNC_HTTP_CLIENT_REPLY_H
#define ASYNC_HTTP_CLIENT_REPLY_H

#include <memory>
#include <QtCore/QObject>
#include <nx/utils/thread/mutex.h>

#include "asynchttpclient.h"


class NX_NETWORK_API QnAsyncHttpClientReply : public QObject {
    Q_OBJECT
public:
    explicit QnAsyncHttpClientReply(const nx_http::AsyncHttpClientPtr &client, QObject *parent = 0);

    nx_http::AsyncHttpClientPtr asyncHttpClient() const;
    bool isFailed() const;
    nx::utils::Url url() const;
    QByteArray contentType();
    QByteArray data();
    nx_http::Response response();

signals:
    void finished(QnAsyncHttpClientReply *reply);

private slots:
    void at_client_done(const nx_http::AsyncHttpClientPtr &client);

private:
    mutable QnMutex m_mutex;

    nx_http::AsyncHttpClientPtr m_client;

    bool m_failed;
    nx::utils::Url m_url;
    nx_http::BufferType m_contentType;
    nx_http::BufferType m_data;
    nx_http::Response m_response;
};

#endif // ASYNC_HTTP_CLIENT_REPLY_H
