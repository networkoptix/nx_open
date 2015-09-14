/**********************************************************
* Sep 8, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CC_CDB_ENDPOINT_FETCHER_H
#define NX_CC_CDB_ENDPOINT_FETCHER_H

#include <functional>

#include <boost/optional.hpp>

#include <QtCore/QString>
#include <QtCore/QUrl>

#include <utils/network/http/asynchttpclient.h>
#include <utils/thread/mutex.h>

namespace nx {
namespace cc {

//!Retrieves url to the specified cloud module
class CloudModuleEndPointFetcher
{
public:
    CloudModuleEndPointFetcher(QString moduleName);
    ~CloudModuleEndPointFetcher();

    //!Specify endpoint explicitely
    void setEndpoint(SocketAddress endpoint);

    //!Retrieves endpoint if unknown. If endpoint is known, then calls \a handler directly from this method
    void get(std::function<void(nx_http::StatusCode::Value, SocketAddress)> handler);

private:
    mutable QnMutex m_mutex;
    boost::optional<SocketAddress> m_endpoint;
    nx_http::AsyncHttpClientPtr m_httpClient;
    QString m_moduleName;

    void onHttpClientDone(
        nx_http::AsyncHttpClientPtr client,
        std::function<void(nx_http::StatusCode::Value, SocketAddress)> handler);
    void parseCloudXml(QByteArray xmlData);
};

}   //cc
}   //nx

#endif  //NX_CC_CDB_ENDPOINT_FETCHER_H
