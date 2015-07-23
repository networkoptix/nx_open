
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
        
        /** Send get request to the given url.
         * @param url                   Target url.
         * @param sucessfullCallback    Callback that should be used in case of success.
         * @param errorCallback         Callback that should be used in case of error.
         * @param timeoutMs             Optional timeout value. 
         *                              Negative value means wait forever.
         *                              Zero means default value (defined in the implementation).
         */
        void sendGet(const QUrl &url
            , const ReplyCallback &sucessfullCallback = ReplyCallback()
            , const ErrorCallback &errorCallback = ErrorCallback()
            , qint64 timeoutMs = HttpClient::kUseDefaultTimeout);
        
        /** Send post request to the given url.
         * @param url                   Target url.
         * @param data                  Post data.
         * @param sucessfullCallback    Callback that should be used in case of success.
         * @param errorCallback         Callback that should be used in case of error.
         * @param timeoutMs             Optional timeout value. 
         *                              Negative value means wait forever.
         *                              Zero means default value (defined in the implementation).
         */
        void sendPost(const QUrl &url
            , const QByteArray &data = QByteArray()
            , const ReplyCallback &successfullCallback = ReplyCallback()
            , const ErrorCallback &errorCallback = ErrorCallback()
            , qint64 timeoutMs = HttpClient::kUseDefaultTimeout);
        
    private:
        class Impl;
        Impl * const m_impl;
    };
}
