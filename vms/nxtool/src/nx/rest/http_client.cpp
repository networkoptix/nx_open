#include "http_client.h"

#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <QtCore/QMap>
#include <QtCore/QPointer>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QAuthenticator>

namespace nx {
namespace rest {

//#define HTTP_CLIENT_DEBUG

namespace {

QNetworkRequest preparePostJsonRequestRequest(const QUrl &url)
{
    QNetworkRequest result(url);
    result.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    return result;
}

// TODO: #tr #ynikitenkov Strings must be generated in the UI class, not in the network one
QString errorCodeToString(QNetworkReply::NetworkError errorCode) {
    switch (errorCode) {
    case QNetworkReply::NetworkError::NoError:
        return QString();
    case QNetworkReply::NetworkError::TimeoutError:
    case QNetworkReply::NetworkError::OperationCanceledError:
        return "Request Timed Out";
    default:
        break;
    }

    return L'#'+QString::number(errorCode);
}

#ifdef HTTP_CLIENT_DEBUG
void logReply(QNetworkReply* reply, const QString &message) {
    if (reply)
        qDebug() << reply << message << reply->url();
    else
        qDebug() << "NULL REPLY" << message;
}
#else
#define logReply(...)
#endif // HTTP_CLIENT_DEBUG

} // namespace

class HttpClient::Impl : public QObject
{
public:
    Impl();
    
    virtual ~Impl();
    
    void sendGet(const QUrl &url
        , qint64 timeoutMs
        , const ReplyCallback &successfulCallback
        , const ErrorCallback &errorCallback
        );

    void sendPost(const QUrl &url
        , qint64 timeoutMs
        , const QByteArray &data
        , const ReplyCallback &sucessfullCallback
        , const ErrorCallback &errorCallback
        );
    
private:
    void setupTimeout(QNetworkReply *reply, qint64 timeoutMs);
    void onReply(QNetworkReply *reply);
    
private:
    struct ReplyInfo {
        ReplyCallback success;
        ErrorCallback error;

        ReplyInfo(ReplyCallback success, ErrorCallback error):
            success(success), error(error)
        {}
    };
    typedef QMap<QNetworkReply *, ReplyInfo> RepliesMap;
    
    QNetworkAccessManager * const m_manager;
    RepliesMap m_replies;
};

HttpClient::Impl::Impl()
    : QObject()
    , m_manager(new QNetworkAccessManager(this))
    , m_replies()
{
    QObject::connect(m_manager, &QNetworkAccessManager::finished, this, &Impl::onReply);
}

HttpClient::Impl::~Impl()
{
}

void HttpClient::Impl::sendGet(const QUrl &url
    , qint64 timeoutMs
    , const ReplyCallback &successfulCallback
    , const ErrorCallback &errorCallback)
{
    QNetworkReply *reply = m_manager->get(QNetworkRequest(url));
    logReply(reply, "sending get command");
    setupTimeout(reply, timeoutMs);
    m_replies.insert(reply
        , ReplyInfo(successfulCallback, errorCallback));
}

void HttpClient::Impl::sendPost(const QUrl &url
    , qint64 timeoutMs
    , const QByteArray &data
    , const ReplyCallback &successfulCallback
    , const ErrorCallback &errorCallback)
{
    QNetworkReply *reply = m_manager->post(preparePostJsonRequestRequest(url), data);
    logReply(reply, "sending post command");
    setupTimeout(reply, timeoutMs);
    m_replies.insert(reply
        , ReplyInfo(successfulCallback, errorCallback));
}

void HttpClient::Impl::setupTimeout(QNetworkReply *reply, qint64 timeoutMs) 
{
    Q_ASSERT(timeoutMs > 0);
    if (timeoutMs <= 0)
        return; //< No timeout - wait forever.

    QPointer<QNetworkReply> replyPtr(reply);
    QTimer::singleShot(timeoutMs, [replyPtr] {
        if (replyPtr) {
            logReply(replyPtr.data(), "aborting request");
            replyPtr->abort(); // here finished() will be emitted and replies will be removed from the m_replies map
        }
    });
}

void HttpClient::Impl::onReply(QNetworkReply *reply)
{
    reply->deleteLater();
    
    RepliesMap::iterator it = m_replies.find(reply);
    if (it == m_replies.end()) {
        logReply(reply, "do not waiting for this reply anymore");
        return;
    }
    
    const QNetworkReply::NetworkError errorCode = reply->error();
    const int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool isRequestError = (errorCode != QNetworkReply::NoError);
    const bool isHttpError = ((httpCode < kHttpSuccessCodeFirst) || (httpCode > kHttpSuccessCodeLast));
    const QByteArray &data = reply->readAll();
    
    logReply(reply, QString("reply received %1 : %2").arg(errorCode).arg(httpCode));
    if (isRequestError || isHttpError)
    { 
        const ErrorCallback &errorCallback = it->error;
        if (errorCallback)
        {
            switch(errorCode)
            {
            case QNetworkReply::AuthenticationRequiredError:
                errorCallback(RequestResult::kUnauthorized);
                break;
            case QNetworkReply::OperationCanceledError:
            case QNetworkReply::TimeoutError:
                errorCallback(RequestResult::kRequestTimeout);
                break;
            case QNetworkReply::NoError:
            {
                if (!isHttpError)
                    errorCallback(RequestResult::kSuccess);
                else if (httpCode == kHttpUnauthorized)
                    errorCallback(RequestResult::kUnauthorized);
                else
                    errorCallback(RequestResult::kUnspecified);

                break;
            }
            default:
                errorCallback(RequestResult::kUnspecified);
            }
        }
    }
    else if (const ReplyCallback &successCallback = it->success)
    {
        successCallback(data);
    }
    m_replies.erase(it);
}

///

HttpClient::HttpClient()
    : m_impl(new Impl())
{
}

HttpClient::~HttpClient()
{
}

void HttpClient::sendGet(const QUrl &url
    , qint64 timeoutMs
    , const ReplyCallback &sucessfullCallback
    , const ErrorCallback &errorCallback)
{
    m_impl->sendGet(url, timeoutMs, sucessfullCallback, errorCallback);    
}

void HttpClient::sendPost(const QUrl &url
    , qint64 timeoutMs
    , const QByteArray &data
    , const ReplyCallback &successfulCallback
    , const ErrorCallback &errorCallback)
{
    m_impl->sendPost(url, timeoutMs, data, successfulCallback, errorCallback);
}

} // namespace rest
} // namespace nx
