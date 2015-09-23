
#pragma once

#include <functional>

#include <base/types.h>
#include <helpers/rest_client.h>

class QUrl;
class QByteArray;

namespace rtu
{
    class HttpClient
    {
    public:
        typedef std::function<void (const QByteArray &data)> ReplyCallback;
        typedef std::function<void (RequestError errorCode)> ErrorCallback;
        
        enum
        {
            kHttpSuccessCodeFirst = 200
            , kHttpSuccessCodeLast = 299
            , kHttpUnauthorized = 401
        };

        HttpClient(int defaultTimeoutMs);
        
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
            , qint64 timeoutMs = RestClient::kUseStandardTimeout);
        
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
            , qint64 timeoutMs = RestClient::kUseStandardTimeout);
        
    private:
        class Impl;
        typedef std::unique_ptr<Impl> ImplPtr;

        const ImplPtr m_impl;
    };
}
