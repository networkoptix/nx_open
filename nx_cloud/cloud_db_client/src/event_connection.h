/**********************************************************
* Apr 29, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <string>

#include <QtCore/QObject>

#include <nx/network/http/asynchttpclient.h>
#include <nx/network/http/multipart_content_parser.h>
#include <nx/network/retry_timer.h>
#include <include/cdb/connection.h>


namespace nx {

namespace network {
namespace cloud {

class CloudModuleEndPointFetcher;

}   //namespace cloud
}   //namespace network

namespace cdb {
namespace cl {

class EventConnection
:
    public QObject,
    public api::EventConnection
{
public:
    EventConnection(
        network::cloud::CloudModuleEndPointFetcher* const endPointFetcher,
        std::string login,
        std::string password);
    virtual ~EventConnection();

    virtual void start(
        api::SystemEventHandlers eventHandlers,
        std::function<void(api::ResultCode)> completionHandler) override;

private:
    enum class State
    {
        init,
        connecting,
        connected,
        reconnecting,
        failed
    };

    network::cloud::CloudModuleEndPointFetcher* const m_cdbEndPointFetcher;
    const std::string m_login;
    const std::string m_password;
    nx_http::AsyncHttpClientPtr m_httpClient;
    api::SystemEventHandlers m_eventHandlers;
    std::function<void(api::ResultCode)> m_connectCompletionHandler;
    std::shared_ptr<nx_http::MultipartContentParser> m_multipartContentParser;
    network::RetryTimer m_reconnectTimer;
    State m_state;

    void cdbEndpointResolved(
        nx_http::StatusCode::Value resCode,
        SocketAddress endpoint);
    void connectionAttemptHasFailed(api::ResultCode result);

private slots:
    void onHttpResponseReceived(nx_http::AsyncHttpClientPtr);
    void onSomeMessageBodyAvailable(nx_http::AsyncHttpClientPtr);
    void onHttpClientDone(nx_http::AsyncHttpClientPtr);
    void onReceivingSerializedEvent(QByteArray serializedEvent);
};

}   //namespace cl
}   //namespace cdb
}   //namespace nx
