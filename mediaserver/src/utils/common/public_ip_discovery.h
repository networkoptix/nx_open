#ifndef __PUBLIC_IP_DISCOVERY_H_
#define __PUBLIC_IP_DISCOVERY_H_

#include <QHostAddress>
#include "utils/network/http/async_http_client_reply.h"

class QnPublicIPDiscovery: public QObject
{
    Q_OBJECT
public:
    QnPublicIPDiscovery();
    void waitForFinished();
    QHostAddress publicIP() const { return m_publicIP; }
public slots:
    void update();
signals:
    void found(const QHostAddress& address);
private:
    void handleReply(const nx_http::AsyncHttpClientPtr& httpClient);
    void sendRequest(const QString &url);
    void nextStage();
private:
    enum class Stage {
        Idle,
        PrimaryUrlsRequesting,
        SecondaryUrlsRequesting,
        PublicIpFound
    };

    QHostAddress m_publicIP;
    Stage m_stage;
    int m_replyInProgress;
    QStringList m_primaryUrls;
    QStringList m_secondaryUrls;
};

#endif // __PUBLIC_IP_DISCOVERY_H_
