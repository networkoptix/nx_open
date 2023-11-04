// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/server/authentication_dispatcher.h>
#include <nx/network/http/server/multi_endpoint_server.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/reflect/instrument.h>

#include "settings.h"

namespace nx::network::http::server::test {
class MultiEndpointServer;
}

namespace nx::network::http::server::test {

struct Response
{
    std::string httpRequest;
};

NX_REFLECTION_INSTRUMENT(Response, (httpRequest))

class View
{
public:
    static constexpr char kHandlerPath[] = "/simpleService/handler";

public:
    View(const Settings& settings);
    ~View();

    void start();
    /**
     * Stops listening ports and terminates all opened connections.
     */
    void stop();

    void doResponse(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);

    std::vector<network::SocketAddress> httpEndpoints() const;
    void setSuccessfullAttemptNum(unsigned num) { m_successAttemptNum = num; };
    void resetAttemptsNum() { m_curAttemptNum = 0; };
    unsigned getCurAttempNum() { return m_curAttemptNum; }

private:
    nx::network::http::server::rest::MessageDispatcher m_httpMessageDispatcher;
    nx::network::http::server::AuthenticationDispatcher m_authenticationDispatcher;
    std::unique_ptr<nx::network::http::server::MultiEndpointServer> m_multiAddressHttpServer;

private:
    void startAcceptor();

private:
    const Settings& m_settings;
    std::atomic<unsigned> m_successAttemptNum = 1;
    std::atomic<unsigned> m_curAttemptNum = 0;
};

} // namespace  nx::network::http::server::test
