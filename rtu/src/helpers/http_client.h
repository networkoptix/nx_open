
#pragma once

#include <functional>

#include <QObject>

class QUrl;
class QByteArray;

namespace rtu
{
    class HttpClient : public QObject
    {
    public:
        typedef std::function<void (const QByteArray &data)> OnSuccesfullReplyFunc;
        typedef std::function<void (const QString &errorReason)> OnErrorReplyFunc;
        
        HttpClient(QObject *parent);
        
        virtual ~HttpClient();
        
        void sendGet(const QUrl &url
            , const OnSuccesfullReplyFunc &sucessfullCallback = OnSuccesfullReplyFunc()
            , const OnErrorReplyFunc &errorCallback = OnErrorReplyFunc());
        
        void sendPost(const QUrl &url
            , const QByteArray &data
            , const OnSuccesfullReplyFunc &successfullCallback
            , const OnErrorReplyFunc &errorCallback = OnErrorReplyFunc());
        
    private:
        class Impl;
        Impl * const m_impl;
    };
}
