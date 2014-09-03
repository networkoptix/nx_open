#include <QNetworkRequest>
#include <QNetworkReply>

#include "public_ip_discovery.h"
#include "media_server/settings.h"
#include "utils/common/sleep.h"
#include "qcoreapplication.h"

static const QString DEFAULT_URL_LIST("http://checkrealip.com; http://networkoptix.com/myip.php; http://checkip.eurodyndns.org");
static const QRegExp iPRegExpr("[^a-zA-Z0-9\\.](([0-9]){1,3}\\.){3}([0-9]){1,3}[^a-zA-Z0-9\\.]");

QnPublicIPDiscovery::QnPublicIPDiscovery():
        m_replyInProgress(0)
{
    connect(&m_networkManager, &QNetworkAccessManager::finished,   this,   &QnPublicIPDiscovery::at_reply_finished);
}

void QnPublicIPDiscovery::update()
{
    QStringList urls = MSSettings::roSettings()->value("publicIPServers", DEFAULT_URL_LIST).toString().split(";");

    for (int i = 0; i < urls.size(); ++i) {
        m_replyInProgress++;
        m_networkManager.get(QNetworkRequest(urls[i].trimmed()));
    }
}

void QnPublicIPDiscovery::waitForFinished()
{
    QTime t;
    t.start();
    while (t.elapsed() < 4000 && m_publicIP.isNull() && m_replyInProgress > 0)
    {
        QnSleep::msleep(1);
        qApp->processEvents();
    }
}

void QnPublicIPDiscovery::at_reply_finished(QNetworkReply * reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = QByteArray(" ") + reply->readAll() + QByteArray(" ");
        int ipPos = iPRegExpr.indexIn(response);
        if (ipPos >= 0) {
            QString result = response.mid(ipPos+1, iPRegExpr.matchedLength()-2);
            if (!result.isEmpty()) {
                QHostAddress newAddress = QHostAddress(result);
                if (newAddress != m_publicIP) {
                    m_publicIP = newAddress;
                    emit found(m_publicIP);
                }
            }
        }
    }
    reply->deleteLater();
    m_replyInProgress--;
}
