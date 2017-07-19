#pragma once

#include <nx/network/http/server/abstract_http_request_handler.h>

#include "proxy_worker.h"
#include "settings.h"

namespace nx {
namespace cloud {
namespace gateway {

namespace conf {

class RunTimeOptions;

} // namespace conf

class ProxyHandler:
    public nx_http::AbstractHttpRequestHandler,
    public AbstractResponseSender
{
public:
    ProxyHandler(
        const conf::Settings& settings,
        const conf::RunTimeOptions& runTimeOptions);

    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const response,
        nx_http::RequestProcessedHandler completionHandler) override;

    virtual void sendResponse(
        nx_http::RequestResult requestResult,
        boost::optional<nx_http::Response> response) override;

private:
    const conf::Settings& m_settings;
    const conf::RunTimeOptions& m_runTimeOptions;

    std::unique_ptr<AbstractStreamSocket> m_targetPeerSocket;
    nx_http::Request m_request;
    nx_http::RequestProcessedHandler m_requestCompletionHandler;
    std::unique_ptr<ProxyWorker> m_requestProxyWorker;
    TargetHost m_targetHost;

    TargetHost cutTargetFromRequest(
        const nx_http::HttpServerConnection& connection,
        nx_http::Request* const request);

    TargetHost cutTargetFromUrl(nx_http::Request* const request);
    TargetHost cutTargetFromPath(nx_http::Request* const request);

    void onConnected(const SocketAddress& targetAddress, SystemError::ErrorCode errorCode);
};

} // namespace gateway
} // namespace cloud
} // namespace nx
