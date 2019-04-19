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
                NX_DEBUG(this, "SSL error: %1 (%2)", e, int(e.error()));

            reply->ignoreSslErrors();
        });
}

void QnWebPage::javaScriptConsoleMessage(const QString& message, int lineNumber, const QString& sourceID)
{
      NX_DEBUG(this, "JS Console: %1:%2 %3", sourceID, lineNumber, message);
}
