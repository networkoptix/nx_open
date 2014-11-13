#include "public_ip_discovery.h"

#include <QtCore/QCoreApplication>

#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

#include <media_server/settings.h>

#include <utils/common/sleep.h>

namespace {
    const QString primaryUrlsList("http://checkrealip.com;http://checkip.eurodyndns.org");
    const QString secondaryUrlsList("http://networkoptix.com/myip");
    const int requestTimeoutMs = 4*1000;
    const QRegExp iPRegExpr("[^a-zA-Z0-9\\.](([0-9]){1,3}\\.){3}([0-9]){1,3}[^a-zA-Z0-9\\.]");
}

#ifdef _DEBUG
//#define QN_PUBLIC_IP_DISCOVERY_DEBUG
#endif

#ifdef QN_PUBLIC_IP_DISCOVERY_DEBUG

QString stage(int value) {
    switch (value) {
    case 0: return "Idle";
    case 1: return "Primary";
    case 2: return "Secondary";
    case 3: return "Found";
    default: 
        Q_ASSERT(false);
        return QString();
    }
}

#define PRINT_DEBUG(MSG) qDebug() << "PUBLIC_IP_DISCOVERY_DEBUG:" << MSG
#else
#define PRINT_DEBUG(MSG) 
#endif 

QnPublicIPDiscovery::QnPublicIPDiscovery():
    m_stage(Stage::Idle),
    m_replyInProgress(0)
{
    connect(&m_networkManager, &QNetworkAccessManager::finished,   this,   &QnPublicIPDiscovery::at_reply_finished);
    m_primaryUrls = MSSettings::roSettings()->value(nx_ms_conf::PUBLIC_IP_SERVERS, primaryUrlsList).toString().split(";", QString::SkipEmptyParts);
    m_secondaryUrls = secondaryUrlsList.split(";", QString::SkipEmptyParts);

    if (m_primaryUrls.isEmpty()) {
        m_primaryUrls = m_secondaryUrls;
        m_secondaryUrls.clear();
    }
    
    PRINT_DEBUG("Primary urls: " + m_primaryUrls.join("; "));
    PRINT_DEBUG("Secondary urls: " + m_secondaryUrls.join("; "));

    Q_ASSERT_X(!m_primaryUrls.isEmpty(), Q_FUNC_INFO, "Server should have at least one public IP url");
}

void QnPublicIPDiscovery::update() {
    m_stage = Stage::PrimaryUrlsRequesting;
    for (const QString &url: m_primaryUrls)
        sendRequest(url);
}

void QnPublicIPDiscovery::waitForFinished() {

    auto wait = [this](int timeout) {
        QTime t;
        t.start();
        while (t.elapsed() < timeout && m_publicIP.isNull() && m_replyInProgress > 0)
        {
            QnSleep::msleep(1);
            qApp->processEvents();
        }
    };

    wait(requestTimeoutMs);

    /* If no public ip found from primary servers, send requests to secondary servers (if any). */
    if (m_stage == Stage::PrimaryUrlsRequesting)
        nextStage();
    

    /* Give additional timeout for secondary servers. */
    if (m_stage == Stage::SecondaryUrlsRequesting) {
        PRINT_DEBUG("Giving additional timeout");
        wait(requestTimeoutMs);
    }
}

void QnPublicIPDiscovery::at_reply_finished(QNetworkReply * reply) {
    handleReply(reply);
    reply->deleteLater();
    m_replyInProgress--;
    if (m_replyInProgress == 0)
        nextStage();

    /* If no public ip found, clean existing. */
    if (m_stage == Stage::Idle && !m_publicIP.isNull()) {
        m_publicIP.clear();
        emit found(m_publicIP);
    }
}

void QnPublicIPDiscovery::handleReply(QNetworkReply* reply) {
    /* Check if reply finished successfully. */
    if (reply->error() != QNetworkReply::NoError)
        return;

    /* Check if reply contents contain any ip address. */
    QByteArray response = QByteArray(" ") + reply->readAll() + QByteArray(" ");
    int ipPos = iPRegExpr.indexIn(response);
    if (ipPos < 0)
        return;

    QString result = response.mid(ipPos+1, iPRegExpr.matchedLength()-2);
    if (result.isEmpty())
        return;

    QHostAddress newAddress = QHostAddress(result);
    if (!newAddress.isNull()) {
        m_stage = Stage::PublicIpFound;
        PRINT_DEBUG("Set stage to " + stage(static_cast<int>(m_stage)));
        if (newAddress != m_publicIP) {
            m_publicIP = newAddress;
            emit found(m_publicIP);
        }
    }
}

void QnPublicIPDiscovery::sendRequest(const QString &url) {
    PRINT_DEBUG("Sending request to: " + url);
    m_replyInProgress++;
    m_networkManager.get(QNetworkRequest(url.trimmed()));
}

void QnPublicIPDiscovery::nextStage() {
    PRINT_DEBUG("Next stage from " + stage(static_cast<int>(m_stage)));
    if (m_stage == Stage::PublicIpFound)
        return;

    if (m_stage == Stage::PrimaryUrlsRequesting && !m_secondaryUrls.isEmpty()) {
        m_stage = Stage::SecondaryUrlsRequesting;
        PRINT_DEBUG("Set stage to " + stage(static_cast<int>(m_stage)));
        for (const QString &url: m_secondaryUrls)
            sendRequest(url);
    } else {
        m_stage = Stage::Idle;
        PRINT_DEBUG("Set stage to " + stage(static_cast<int>(m_stage)));
    }
}
