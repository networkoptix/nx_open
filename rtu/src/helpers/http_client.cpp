
#include "http_client.h"

#include <QMap>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QAuthenticator>

namespace
{
    QNetworkRequest preparePostJsonReqiestRequest(const QUrl &url)
    {
        QNetworkRequest result(url);
        result.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        return result;
    }
}

class rtu::HttpClient::Impl : public QObject
{
public:
    Impl(QObject *parent);
    
    virtual ~Impl();
    
    void sendGet(const QUrl &url
        , const ReplyCallback &successfullCallback
        , const ErrorCallback &errorCallback);

    void sendPost(const QUrl &url
        , const QByteArray &data
        , const ReplyCallback &sucessfullCallback
        , const ErrorCallback &errorCallback);
    
private:
    void onReply(QNetworkReply *reply);
    
private:
    typedef QPair<ReplyCallback, ErrorCallback> CallbacksPair;
    typedef QMap<QNetworkReply *, CallbacksPair> RepliesMap;
    
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
    , const ErrorCallback &errorCallback)
{
    QNetworkReply *reply = m_manager->get(QNetworkRequest(url));
   // qDebug() << reply << " sending command: " << url; 
    m_replies.insert(reply
        , CallbacksPair(successfullCallback, errorCallback));
}

void rtu::HttpClient::Impl::sendPost(const QUrl &url
    , const QByteArray &data
    , const ReplyCallback &successfullCallback
    , const ErrorCallback &errorCallback)
{
    m_replies.insert(m_manager->post(preparePostJsonReqiestRequest(url), data)
        , CallbacksPair(successfullCallback, errorCallback));
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
    
    const int errorCode = reply->error();
    const int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool isRequestError = (errorCode != QNetworkReply::NoError);
    const bool isHttpError = ((httpCode < kHttpSuccessCodeFirst) || (httpCode > kHttpSuccessCodeLast));
    const QByteArray &data = reply->readAll();
    
//    qDebug() << reply << " netowrk reply: " << errorCode << " : " << httpCode;
//    qDebug() << data;
    if (isRequestError || isHttpError)
    { 
        const ErrorCallback &errorCallback = it->second;
        if (errorCallback)
        {
            const QVariant &httpErrorVar = 
                reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute);
            const QString &httpError = (httpErrorVar.isValid() ? 
                httpErrorVar.toString() : QString());
            const QString &errorReason = (!httpError.isEmpty()? httpError 
                : QString("Netowrk error #%1").arg(errorCode));
            errorCallback(errorReason, httpCode);
        }
    }
    else if (const ReplyCallback &successCallback = it->first)
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
    , const ErrorCallback &errorCallback)
{
    m_impl->sendGet(url, sucessfullCallback, errorCallback);    
}

void rtu::HttpClient::sendPost(const QUrl &url
    , const QByteArray &data
    , const ReplyCallback &successfullCallback
    , const ErrorCallback &errorCallback)
{
    m_impl->sendPost(url, data, successfullCallback, errorCallback);
}


