#include "server_aliveness_tester.h"

#include "abstract_async_client.h"

namespace nx::network::stun {

ServerAlivenessTester::ServerAlivenessTester(
    KeepAliveOptions keepAliveOptions,
    AbstractAsyncClient* client)
    :
    base_type(keepAliveOptions),
    m_client(client)
{
}

ServerAlivenessTester::~ServerAlivenessTester()
{
    if (isInSelfAioThread())
        cancelProbe();
}

void ServerAlivenessTester::probe(ProbeResultHandler handler)
{
    nx::network::stun::Message probe(
        stun::Header(
            nx::network::stun::MessageClass::request,
            nx::network::stun::bindingMethod));

    m_client->sendRequest(
        std::move(probe),
        [this, handler = std::move(handler)](auto&&... args) mutable
        {
            processResponse(
                std::move(handler),
                std::forward<decltype(args)>(args)...);
        },
        this);
}

void ServerAlivenessTester::cancelProbe()
{
    m_client->cancelHandlersSync(this);
}

void ServerAlivenessTester::processResponse(
    ProbeResultHandler handler,
    SystemError::ErrorCode resultCode,
    Message /*message*/)
{
    handler(resultCode == SystemError::noError);
}

} // namespace nx::network::stun
