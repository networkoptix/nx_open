/**********************************************************
* Sep 8, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CC_CDB_ENDPOINT_FETCHER_H
#define NX_CC_CDB_ENDPOINT_FETCHER_H

#include <functional>
#include <vector>

#include <boost/optional.hpp>

#include <QtCore/QString>
#include <QtCore/QUrl>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/utils/thread/mutex.h>

#include "endpoint_selector.h"


namespace nx {
namespace network {
namespace cloud {

//!Retrieves url to the specified cloud module
class NX_NETWORK_API CloudModuleEndPointFetcher
:
    public aio::BasicPollable
{
public:
    CloudModuleEndPointFetcher(
        QString moduleName,
        std::unique_ptr<AbstractEndpointSelector> endpointSelector);
    ~CloudModuleEndPointFetcher();

    virtual void stopWhileInAioThread() override;

    //!Specify endpoint explicitely
    void setEndpoint(SocketAddress endpoint);
    //!Retrieves endpoint if unknown. If endpoint is known, then calls \a handler directly from this method
    void get(std::function<void(nx_http::StatusCode::Value, SocketAddress)> handler);

private:
    mutable QnMutex m_mutex;
    boost::optional<SocketAddress> m_endpoint;
    nx_http::AsyncHttpClientPtr m_httpClient;
    QString m_moduleName;
    std::vector<std::function<void(nx_http::StatusCode::Value, SocketAddress)>> m_resolveHandlers;
    std::unique_ptr<AbstractEndpointSelector> m_endpointSelector;
    bool m_requestIsRunning;

    void onHttpClientDone(nx_http::AsyncHttpClientPtr client);
    bool parseCloudXml(
        QByteArray xmlData,
        std::vector<SocketAddress>* const endpoints);
    void signalWaitingHandlers(
        QnMutexLockerBase* const lk,
        nx_http::StatusCode::Value statusCode,
        const SocketAddress& endpoint);
    void endpointSelected(
        nx_http::StatusCode::Value result,
        SocketAddress selectedEndpoint);
};

} // namespace cloud
} // namespace network
} // namespace nx

#endif  //NX_CC_CDB_ENDPOINT_FETCHER_H
