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

public slots:
    void update();

signals:
    void found(const QHostAddress& address);

private:
    void handleReply(const nx_http::AsyncHttpClientPtr& httpClient);
    void sendRequest(const QString &url);
    void nextStage();

private:
    enum class Stage
    {
        idle,
        primaryUrlsRequesting,
        secondaryUrlsRequesting,
        publicIpFound
    };

    QHostAddress m_publicIP;
    Stage m_stage;
    int m_replyInProgress;
    QStringList m_primaryUrls;
    QStringList m_secondaryUrls;
    QnMutex m_mutex;
    std::set<nx_http::AsyncHttpClientPtr> m_httpRequests;

    virtual void stopWhileInAioThread() override;

    QString toString(Stage value) const;
};

} // namespace network
} // namespace nx
