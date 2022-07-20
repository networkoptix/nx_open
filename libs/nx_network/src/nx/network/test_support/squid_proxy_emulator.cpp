// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "squid_proxy_emulator.h"

#include <nx/network/http/server/proxy/proxy_handler.h>
#include <nx/utils/byte_stream/string_replacer.h>

namespace nx::network::test {

namespace {

class ProxyHandler:
    public http::server::proxy::AbstractProxyHandler
{
public:
    ProxyHandler(const SocketAddress& targetAddress):
        m_targetAddress(targetAddress)
    {
    }

protected:
    virtual void detectProxyTarget(
        const nx::network::http::ConnectionAttrs& /*connAttrs*/,
        const nx::network::SocketAddress& /*requestSource*/,
        nx::network::http::Request* const /*request*/,
        ProxyTargetDetectedHandler handler) override
    {
        handler(
            nx::network::http::StatusCode::ok,
            TargetHost(m_targetAddress));
    }

private:
    const SocketAddress m_targetAddress;
};

} // namespace

//-------------------------------------------------------------------------------------------------

bool SquidProxyEmulator::start()
{
    m_httpServer.registerRequestProcessor(
        http::kAnyPath,
        [this]()
        {
            return std::make_unique<ProxyHandler>(m_targetAddress);
        });
    return m_httpServer.bindAndListen();
}

void SquidProxyEmulator::setDestination(const SocketAddress& targetAddress)
{
    m_targetAddress = targetAddress;
}

//-------------------------------------------------------------------------------------------------

namespace deprecated {

SquidProxyEmulator::SquidProxyEmulator()
{
    setUpStreamConverterFactory(
        []() { return std::make_unique<nx::utils::bstream::StringReplacer>(
            "Connection: Upgrade\r\n", ""); });
}

} // namespace deprecated

} // namespace nx::network::test
