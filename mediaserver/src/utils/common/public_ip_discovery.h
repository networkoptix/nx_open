#ifndef __PUBLIC_IP_DISCOVERY_H_
#define __PUBLIC_IP_DISCOVERY_H_

#include <QNetworkAccessManager>
#include <QHostAddress>

class QNetworkAccessManager;

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
    QNetworkAccessManager m_networkManager;
    QHostAddress m_publicIP;
    int m_replyInProgress;
};

#endif // __PUBLIC_IP_DISCOVERY_H_
