// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../cloud/speed_test/http_api_paths.h"
#include "../http/http_types.h"
#include "../http/server/rest/http_server_rest_message_dispatcher.h"
#include "../http/server/request_processing_types.h"

namespace nx::network::cloud::speed_test::test {

class NX_NETWORK_API UplinkSpeedTestServer
{
public:
    /**
     * Registers the speed test endpoint under {basePath}/speedtest/bandwidth.
     * NOTE: Must be called before requestPath().
     * NOTE: Both ping test and bandwidth test use the same endpoint.
     */
    void registerRequestHandlers(
        const std::string& basePath,
        network::http::server::rest::MessageDispatcher* messageDispatcher);

    /**
     * @return the requestPath as registered in registerRequestHandlers().
     */
    const std::string& requestPath() const;

    /**
     * Install an event handler that fires when a bandwidth test starts with the current test
     * sequence.
     */
    void setBandwidthTestInProgressHandler(
        nx::utils::MoveOnlyFunc<void(int /*xTestSequence*/)> handler);

private:
    void serve(
        nx::network::http::RequestContext request,
        nx::network::http::RequestProcessedHandler completionHandler);

private:
    std::string m_speedTestRequestPath;
    nx::utils::MoveOnlyFunc<void(int /*xTestSequence*/)> m_bandwidthTestInProgressHandler;
};

} // namespace nx::network::cloud::speed_test::test
