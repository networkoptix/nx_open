
#pragma once

#include <memory>
#include <functional>
#include <QtGlobal>
#include <QByteArray>

class QUrl;
class QByteArray;

namespace nx {
namespace rest {

class HttpClient
{
public:
    enum class RequestResult
    {              
        kSuccess,
        kRequestTimeout,
        kUnauthorized,
        kUnspecified,
    };

    typedef std::function<void (const QByteArray &data)> ReplyCallback;
    typedef std::function<void (RequestResult errorCode)> ErrorCallback;
        
    enum
    {
        kHttpSuccessCodeFirst = 200
        , kHttpSuccessCodeLast = 299
        , kHttpUnauthorized = 401
    };

    HttpClient();
    virtual ~HttpClient();
        
    /**
        * Send get request to the given url.
        * @param timeoutMs             Timeout. Should be greater than zero.
        * @param url                   Target url.
        * @param successfulCallback    Callback that should be used in case of success.
        * @param errorCallback         Callback that should be used in case of error.
        */
    void sendGet(const QUrl &url
        , qint64 timeoutMs
        , const ReplyCallback &successfulCallback = ReplyCallback()
        , const ErrorCallback &errorCallback = ErrorCallback()          );
        
    /**
        * Send post request to the given url.
        * @param timeoutMs             Timeout. Should be greater than zero.
        * @param url                   Target url.
        * @param data                  Post data.
        * @param sucessfulCallback    Callback that should be used in case of success.
        * @param errorCallback         Callback that should be used in case of error.
        */
    void sendPost(const QUrl &url
        , qint64 timeoutMs
        , const QByteArray &data = QByteArray()
        , const ReplyCallback &successfulCallback = ReplyCallback()
        , const ErrorCallback &errorCallback = ErrorCallback());
        
private:
    class Impl;
    const std::unique_ptr<Impl> m_impl;
};

} // namespace rest
} // namespace nx
