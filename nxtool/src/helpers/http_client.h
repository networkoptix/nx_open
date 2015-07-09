
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
        typedef std::function<void (const QByteArray &data)> ReplyCallback;
        typedef std::function<void (const QString &errorReason
            , int errorCode)> ErrorCallback;
        
        enum { kUseDefaultTimeout = 0 };

        HttpClient(QObject *parent);
        
        virtual ~HttpClient();
        
        //TODO: #gdm describe default values for timeout
        void sendGet(const QUrl &url
            , const ReplyCallback &sucessfullCallback = ReplyCallback()
            , const ErrorCallback &errorCallback = ErrorCallback()
            , qint64 timeoutMs = kUseDefaultTimeout);
        
        //TODO: #gdm describe default values for timeout
        void sendPost(const QUrl &url
            , const QByteArray &data
            , const ReplyCallback &successfullCallback
            , const ErrorCallback &errorCallback = ErrorCallback()
            , qint64 timeoutMs = kUseDefaultTimeout);
        
    private:
        class Impl;
        Impl * const m_impl;
    };
}
