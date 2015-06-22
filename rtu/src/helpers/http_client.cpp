
#include "http_client.h"

#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <QtCore/QMap>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QAuthenticator>

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

    const qint64 defaultRequestTimeoutMs = 10*1000;
    const qint64 timerPrecisionMs = 500;
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
    void onReply(QNetworkReply *reply);
    
private:
    struct ReplyInfo {
        ReplyCallback success;
        ErrorCallback error;
        qint64 timeoutTimeMs;

        ReplyInfo(ReplyCallback success, ErrorCallback error, qint64 timeoutMs):
            success(success), error(error)
        {
            //TODO: #gdm describe default values
            if (timeoutMs == 0)
                timeoutMs = defaultRequestTimeoutMs;
            timeoutTimeMs = QDateTime::currentMSecsSinceEpoch() + timeoutMs;
        }
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

    QTimer *timeoutTimer = new QTimer(this);
    timeoutTimer->setInterval(timerPrecisionMs);
    timeoutTimer->setSingleShot(false);
    connect(timeoutTimer, &QTimer::timeout, this, [this] {

        QList<QNetworkReply*> timedOutReplies;
        qint64 time = QDateTime::currentMSecsSinceEpoch();
        for (auto it = m_replies.cbegin(); it != m_replies.cend(); ++it) {
            if (time >= it->timeoutTimeMs)
                timedOutReplies.push_back(it.key());
        }
        for (auto reply: timedOutReplies) {
//            qDebug() << "Aborting reply";
            reply->abort(); // here finished() will be emitted and replies will be removed from the m_replies map 
        }

    });
    timeoutTimer->start();
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
//    qDebug() << reply << " sending command: " << url; 
    m_replies.insert(reply
        , ReplyInfo(successfullCallback, errorCallback, timeoutMs));
}

void rtu::HttpClient::Impl::sendPost(const QUrl &url
    , const QByteArray &data
    , const ReplyCallback &successfullCallback
    , const ErrorCallback &errorCallback
    , qint64 timeoutMs)
{
    QNetworkReply *reply = m_manager->post(preparePostJsonRequestRequest(url), data);
//    qDebug() << reply << "sending post command: " << url; 
    m_replies.insert(reply
        , ReplyInfo(successfullCallback, errorCallback, timeoutMs));
}

void rtu::HttpClient::Impl::onReply(QNetworkReply *reply)
{
    reply->deleteLater();
    
    RepliesMap::iterator it = m_replies.find(reply);
    if (it == m_replies.end())
        return;
    
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
    
//    qDebug() << reply << " network reply: " << errorCode << " : " << httpCode;
//    qDebug() << data;
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
            const QString &errorReason = (!httpError.isEmpty()? httpError 
                : QString("Network Error: %1").arg(errorCodeToString(errorCode)));
            errorCallback(errorReason, httpCode);
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


