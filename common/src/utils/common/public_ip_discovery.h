#pragma once

#include <set>

#include <QtNetwork/QHostAddress>

#include <nx/network/http/async_http_client_reply.h>
#include <nx/utils/thread/mutex.h>

class QnPublicIPDiscovery:
    public QObject
{
    Q_OBJECT

public:
    /** If \a primaryUrls is empty, default urls are used */
    QnPublicIPDiscovery(QStringList primaryUrls = QStringList());
    virtual ~QnPublicIPDiscovery() override;

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

    QString toString(Stage value) const;
};
