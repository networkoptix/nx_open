
#include "http_client.h"

#include <QMap>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QAuthenticator>

namespace
{
    struct ReplyData
    {
        rtu::HttpClient::OnSuccesfullReplyFunc successfullCallbac;
        rtu::HttpClient::OnErrorReplyFunc errorCallback;
    };
    
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
        , const OnSuccesfullReplyFunc &successfullCallback
        , const OnErrorReplyFunc &errorCallback);

    void sendPost(const QUrl &url
        , const QByteArray &data
        , const OnSuccesfullReplyFunc &sucessfullCallback
        , const OnErrorReplyFunc &errorCallback);
    
private:
    void onReply(QNetworkReply *reply);
    
private:
    typedef QMap<QNetworkReply *, ReplyData> RepliesMap;
    
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
    , const OnSuccesfullReplyFunc &successfullCallback
    , const OnErrorReplyFunc &errorCallback)
{
    m_replies.insert(m_manager->get(QNetworkRequest(url)), { successfullCallback, errorCallback });
}

void rtu::HttpClient::Impl::sendPost(const QUrl &url
    , const QByteArray &data
    , const OnSuccesfullReplyFunc &successfullCallback
    , const OnErrorReplyFunc &errorCallback)
{
    m_replies.insert(m_manager->post(preparePostJsonReqiestRequest(url), data)
        , { successfullCallback, errorCallback });
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
    
    const int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if ((httpCode < kHttpSuccessCodeFirst) || (httpCode > kHttpSuccessCodeLast))
    { 
        if (it->errorCallback)
            it->errorCallback(reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString());
    }
    else
    {
        if (it->successfullCallbac)
            it->successfullCallbac(reply->readAll());
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
    , const OnSuccesfullReplyFunc &sucessfullCallback
    , const OnErrorReplyFunc &errorCallback)
{
    m_impl->sendGet(url, sucessfullCallback, errorCallback);    
}

void rtu::HttpClient::sendPost(const QUrl &url
    , const QByteArray &data
    , const OnSuccesfullReplyFunc &successfullCallback
    , const OnErrorReplyFunc &errorCallback)
{
    m_impl->sendPost(url, data, successfullCallback, errorCallback);
}


