
#include "http_client.h"

#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <QtCore/QMap>
#include <QtCore/QPointer>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QAuthenticator>

//#define HTTP_CLIENT_DEBUG

namespace
{
    QNetworkRequest preparePostJsonRequestRequest(const QUrl &url)
    {
        QNetworkRequest result(url);
        result.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        return result;
    }

    //TODO: #tr #ynikitenkov Strings must be generated in the UI class, not in the network one
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
#endif

    const qint64 defaultRequestTimeoutMs = 10 * 1000;
}

class rtu::HttpClient::Impl : public QObject
{
public:
    Impl(QObject *parent);
    
    virtual ~Impl();
    
    void sendGet(const QUrl &url
        , const ReplyCallback &successfullCallback
        , const ErrorCallback &errorCallback
        , qint64 timeoutMs
        );

    void sendPost(const QUrl &url
        , const QByteArray &data
        , const ReplyCallback &sucessfullCallback
        , const ErrorCallback &errorCallback
        , qint64 timeoutMs
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

rtu::HttpClient::Impl::Impl(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_replies()
{
    QObject::connect(m_manager, &QNetworkAccessManager::finished, this, &Impl::onReply);
}

rtu::HttpClient::Impl::~Impl()
{
}

void rtu::HttpClient::Impl::sendGet(const QUrl &url
    , const ReplyCallback &successfullCallback
    , const ErrorCallback &errorCallback
    , qint64 timeoutMs)
{
    QNetworkReply *reply = m_manager->get(QNetworkRequest(url));
    logReply(reply, "sending get command");
    setupTimeout(reply, timeoutMs);
    m_replies.insert(reply
        , ReplyInfo(successfullCallback, errorCallback));
}

void rtu::HttpClient::Impl::sendPost(const QUrl &url
    , const QByteArray &data
    , const ReplyCallback &successfullCallback
    , const ErrorCallback &errorCallback
    , qint64 timeoutMs)
{
    QNetworkReply *reply = m_manager->post(preparePostJsonRequestRequest(url), data);
    logReply(reply, "sending post command");
    setupTimeout(reply, timeoutMs);
    m_replies.insert(reply
        , ReplyInfo(successfullCallback, errorCallback));
}

void rtu::HttpClient::Impl::setupTimeout(QNetworkReply *reply, qint64 timeoutMs) {
    if (timeoutMs < kUseDefaultTimeout)
        return;

    if (timeoutMs == kUseDefaultTimeout)
        timeoutMs = defaultRequestTimeoutMs;

    QPointer<QNetworkReply> replyPtr(reply);
    QTimer::singleShot(timeoutMs, [replyPtr] {
        if (replyPtr) {
            logReply(replyPtr.data(), "aborting request");
            replyPtr->abort(); // here finished() will be emitted and replies will be removed from the m_replies map
        }
    });
}

void rtu::HttpClient::Impl::onReply(QNetworkReply *reply)
{
    reply->deleteLater();
    
    RepliesMap::iterator it = m_replies.find(reply);
    if (it == m_replies.end()) {
        logReply(reply, "do not waiting for this reply anymore");
        return;
    }
    
    enum
    {
        kHttpSuccessCodeFirst = 200
        , kHttpSuccessCodeLast = 299
    };
    
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
            const QVariant &httpErrorVar = 
                reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute);
            const QString &httpError = (httpErrorVar.isValid() ? 
                httpErrorVar.toString() : QString());

            //TODO: #tr #ynikitenkov Strings must be generated in the UI class, not in the network one
//            const QString &errorReason = (!httpError.isEmpty()? httpError 
//                : QString("Network Error: %1").arg(errorCodeToString(errorCode)));
            errorCallback(httpCode);
        }
    }
    else if (const ReplyCallback &successCallback = it->success)
    {
        successCallback(data);
    }
    m_replies.erase(it);
}

///

rtu::HttpClient::HttpClient(QObject *parent)
    : QObject(parent)
    , m_impl(new Impl(this))
{
}

rtu::HttpClient::~HttpClient()
{
}

void rtu::HttpClient::sendGet(const QUrl &url
    , const ReplyCallback &sucessfullCallback
    , const ErrorCallback &errorCallback
    , qint64 timeoutMs)
{
    m_impl->sendGet(url, sucessfullCallback, errorCallback, timeoutMs);    
}

void rtu::HttpClient::sendPost(const QUrl &url
    , const QByteArray &data
    , const ReplyCallback &successfullCallback
    , const ErrorCallback &errorCallback
    , qint64 timeoutMs)
{
    m_impl->sendPost(url, data, successfullCallback, errorCallback, timeoutMs);
}


