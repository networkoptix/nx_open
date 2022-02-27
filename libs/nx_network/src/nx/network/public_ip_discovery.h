// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <QtNetwork/QHostAddress>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/async_http_client_reply.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace network {

class NX_NETWORK_API PublicIPDiscovery:
    public QObject,
    public nx::network::aio::BasicPollable
{
    Q_OBJECT

public:
    /** If primaryUrls is empty, default urls are used */
    PublicIPDiscovery(QStringList primaryUrls = QStringList());
    virtual ~PublicIPDiscovery() override;

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    void waitForFinished();
    QHostAddress publicIP() const;

    /**
     * Use this method when the public IP is known from a different source.
     */
    void setPublicIp(const QHostAddress& ip);

public slots:
    void update();

signals:
    void found(const QHostAddress& address);

private:
    void handleReply(const nx::network::http::AsyncHttpClientPtr& httpClient);
    void sendRequestUnsafe(const QString &url);
    void nextStage();
    int requestsInProgress() const;
private:
    enum class Stage
    {
        idle,
        primaryUrlsRequesting,
        secondaryUrlsRequesting,
        publicIpFound
    };
    void setStage(Stage value);

    QHostAddress m_publicIP;
    Stage m_stage;
    QStringList m_primaryUrls;
    QStringList m_secondaryUrls;
    mutable nx::Mutex m_mutex;
    std::set<nx::network::http::AsyncHttpClientPtr> m_httpRequests;

    virtual void stopWhileInAioThread() override;

    QString toString(Stage value) const;
};

} // namespace network
} // namespace nx
