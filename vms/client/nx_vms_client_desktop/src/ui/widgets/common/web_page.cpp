#include "web_page.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <nx/utils/log/log.h>

namespace {

/**
 * Explicitly forbid HTTP2 connections.
 */
class Http1NetworkAccessManager: public QNetworkAccessManager
{
public:
    using QNetworkAccessManager::QNetworkAccessManager;

protected:
    virtual QNetworkReply* createRequest(Operation op, const QNetworkRequest& request,
        QIODevice* outgoingData = nullptr)
    {
        QNetworkRequest copy(request);
        copy.setAttribute(QNetworkRequest::HTTP2AllowedAttribute, false);
        return QNetworkAccessManager::createRequest(op, copy, outgoingData);
    }
};

} // namespace

QnWebPage::QnWebPage(QObject* parent): base_type(parent)
{
    setNetworkAccessManager(new Http1NetworkAccessManager(this));

    connect(networkAccessManager(), &QNetworkAccessManager::sslErrors, this,
        [this](QNetworkReply* reply, const QList<QSslError>& errors)
        {
            for (auto e: errors)
                NX_DEBUG(this, "SSL error: %1 (%2)", e, int(e.error()));

            reply->ignoreSslErrors();
        });
}

void QnWebPage::javaScriptConsoleMessage(
    const QString& message, int lineNumber, const QString& sourceID)
{
    NX_DEBUG(this, "JS Console: %1:%2 %3", sourceID, lineNumber, message);
}
