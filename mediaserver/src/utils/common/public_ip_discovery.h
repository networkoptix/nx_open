#ifndef __PUBLIC_IP_DISCOVERY_H_
#define __PUBLIC_IP_DISCOVERY_H_

#include <QNetworkAccessManager>
#include <QHostAddress>

class QNetworkAccessManager;
class QNetworkReply;

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
private slots:
    void at_reply_finished(QNetworkReply* reply);

private:
    void handleReply(QNetworkReply* reply);
    void sendRequest(const QString &url);
    void nextStage();
private:
    enum class Stage {
        Idle,
        PrimaryUrlsRequesting,
        SecondaryUrlsRequesting,
        PublicIpFound
    };

    QNetworkAccessManager m_networkManager;
    QHostAddress m_publicIP;
    Stage m_stage;
    int m_replyInProgress;
    QStringList m_primaryUrls;
    QStringList m_secondaryUrls;
};

#endif // __PUBLIC_IP_DISCOVERY_H_
