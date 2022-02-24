// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    std::string contentType();
    nx::Buffer data();
    nx::network::http::Response response();

signals:
    void finished(QnAsyncHttpClientReply* reply);

private slots:
    void at_client_done(const nx::network::http::AsyncHttpClientPtr& client);

private:
    mutable nx::Mutex m_mutex;

    nx::network::http::AsyncHttpClientPtr m_client;

    bool m_failed;
    nx::utils::Url m_url;
    std::string m_contentType;
    nx::Buffer m_data;
    nx::network::http::Response m_response;
};
