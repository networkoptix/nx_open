/**********************************************************
* 9 sep 2014
* a.kolesnikov
***********************************************************/

#pragma once

#include <functional>

#include <nx/network/cloud/data/listen_data.h>
#include <nx/network/cloud/data/resolve_data.h>
#include <nx/network/cloud/data/result_code.h>
#include <utils/serialization/lexical.h>

#include "request_processor.h"


namespace nx {

namespace stun {
class MessageDispatcher;
}

namespace hpm {

class ListeningPeerPool;

/** Registers peers which desire to accept cloud connections, resolves such peers address.
 */
class PeerRegistrator
        : protected RequestProcessor
{
public:
    PeerRegistrator(
        AbstractCloudDataProvider* cloudData,
        nx::stun::MessageDispatcher* dispatcher,
        ListeningPeerPool* const listeningPeerPool);

    void bind(
        const ConnectionStrongRef& connection,
        stun::Message message);
    void listen(
        const ConnectionStrongRef& connection,
        stun::Message message);
    void resolve(
        const ConnectionStrongRef& connection,
        api::ResolveRequest request,
        std::function<void(api::ResultCode, api::ResolveResponse)> completionHandler);
    //void connect(
    //    const ConnectionStrongRef& connection,
    //    stun::Message message);

    //void connectionResult(
    //    const ConnectionStrongRef& connection,
    //    api::ConnectionResultRequest request,
    //    std::function<void(api::ResultCode)> completionHandler);

private:
    ListeningPeerPool* const m_listeningPeerPool;
};

} // namespace hpm
} // namespace nx
