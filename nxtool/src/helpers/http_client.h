
#pragma once

#include <functional>

#include <QObject>

#include <base/types.h>

class QUrl;
class QByteArray;

namespace rtu
{
    class HttpClient : public QObject
    {
    public:
        typedef std::function<void (const QByteArray &data)> ReplyCallback;
        typedef std::function<void (RequestError errorCode)> ErrorCallback;
        
        enum { kUseDefaultTimeout = -1 };
        enum { kDefaultTimeoutMs = 10 * 1000 };

        enum
        {
            kHttpSuccessCodeFirst = 200
            , kHttpSuccessCodeLast = 299
            , kHttpUnauthorized = 401
        };

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
