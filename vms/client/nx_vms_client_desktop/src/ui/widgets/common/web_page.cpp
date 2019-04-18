#include "web_page.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <nx/utils/log/log.h>


QnWebPage::QnWebPage(QObject* parent):
    base_type(parent)
{
    connect(this->networkAccessManager(), &QNetworkAccessManager::sslErrors, this,
        [this](QNetworkReply* reply, const QList<QSslError>& errors)
        {
            for (auto e: errors)
                NX_INFO(this, "SSL error: %1 (%2)", e.errorString(), int(e.error()));

            reply->ignoreSslErrors(
                QList{QSslError(QSslError::SelfSignedCertificate, reply->sslConfiguration().peerCertificate()),
                QSslError(QSslError::HostNameMismatch, reply->sslConfiguration().peerCertificate())});
        });
}

void QnWebPage::javaScriptConsoleMessage(const QString& message, int lineNumber, const QString& sourceID)
{
      NX_INFO(this, "JS Console: %1:%2 %3", sourceID, lineNumber, message);
}
