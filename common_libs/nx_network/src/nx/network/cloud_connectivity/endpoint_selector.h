/**********************************************************
* Oct 7, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CC_CDB_ENDPOINT_SELECTOR_H
#define NX_CC_CDB_ENDPOINT_SELECTOR_H

#include <functional>
#include <vector>

#include <nx/network/socket_common.h>
#include <nx/network/http/httptypes.h>

#include <QtCore/QString>


namespace nx {
namespace cc {

//!Selects endpoint that is "best" due to some logic
class NX_NETWORK_API AbstractEndpointSelector
{
public:
    virtual ~AbstractEndpointSelector() {}

    /*!
        \note Multiple calls are allowed
    */
    virtual void selectBestEndpont(
        const QString& moduleName,
        std::vector<SocketAddress> endpoints,
        std::function<void(nx_http::StatusCode::Value, SocketAddress)> handler) = 0;
};

class NX_NETWORK_API RandomEndpointSelector
:
    public AbstractEndpointSelector
{
public:
    /*!
        \param \a handler Called directly in this method
    */
    virtual void selectBestEndpont(
        const QString& moduleName,
        std::vector<SocketAddress> endpoints,
        std::function<void(nx_http::StatusCode::Value, SocketAddress)> handler) override;
};

}   //cc
}   //nx

#endif  //NX_CC_CDB_ENDPOINT_SELECTOR_H
