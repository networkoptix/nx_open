#pragma once

#include <memory>

#include <QtCore/QObject>

#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/utils/thread/mutex.h>

class NX_NETWORK_API QnAsyncHttpClientReply:
    public QObject
{
    Q_OBJECT

public:
    explicit QnAsyncHttpClientReply(
        const nx::network::http::AsyncHttpClientPtr& client,
        QObject* parent = 0);

    nx::network::http::AsyncHttpClientPtr asyncHttpClient() const;
    bool isFailed() const;
    nx::utils::Url url() const;
    QByteArray contentType();
    QByteArray data();
    nx::network::http::Response response();

signals:
    void finished(QnAsyncHttpClientReply* reply);

private slots:
    void at_client_done(const nx::network::http::AsyncHttpClientPtr& client);

private:
    mutable QnMutex m_mutex;

    nx::network::http::AsyncHttpClientPtr m_client;

    bool m_failed;
    nx::utils::Url m_url;
    nx::network::http::BufferType m_contentType;
    nx::network::http::BufferType m_data;
    nx::network::http::Response m_response;
};
