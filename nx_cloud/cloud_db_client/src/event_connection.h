/**********************************************************
* Apr 29, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <string>

#include <QtCore/QObject>

#include <nx/network/cloud/cdb_endpoint_fetcher.h>
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

class EventConnection:
    public QObject,
    public api::EventConnection
{
public:
    EventConnection(
        network::cloud::CloudModuleEndPointFetcher* const endPointFetcher);
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

    std::unique_ptr<network::cloud::CloudModuleEndPointFetcher::ScopedOperation>
        m_cdbEndPointFetcher;
    nx_http::AuthInfo m_auth;
    nx_http::AsyncHttpClientPtr m_httpClient;
    api::SystemEventHandlers m_eventHandlers;
    std::function<void(api::ResultCode)> m_connectCompletionHandler;
    std::shared_ptr<nx_http::MultipartContentParser> m_multipartContentParser;
    network::RetryTimer m_reconnectTimer;
    State m_state;
    SocketAddress m_cdbEndpoint;

    void cdbEndpointResolved(
        nx_http::StatusCode::Value resCode,
        SocketAddress endpoint);
    void initiateConnection();
    void connectionAttemptHasFailed(api::ResultCode result);

    virtual void setCredentials(const std::string& login, const std::string& password) override;
    virtual void setProxyCredentials(const std::string& login, const std::string& password) override;
    virtual void setProxyVia(
        const std::string& proxyHost,
        std::uint16_t proxyPort) override;

private slots:
    void onHttpResponseReceived(nx_http::AsyncHttpClientPtr);
    void onSomeMessageBodyAvailable(nx_http::AsyncHttpClientPtr);
    void onHttpClientDone(nx_http::AsyncHttpClientPtr);
    void onReceivingSerializedEvent(QByteArray serializedEvent);
};

}   //namespace cl
}   //namespace cdb
}   //namespace nx
