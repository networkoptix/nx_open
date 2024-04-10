// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <list>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/system_socket.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/move_only_func.h>

#include "../abstract_tunnel_connector.h"
#include "tunnel_tcp_abstract_endpoint_verificator.h"

namespace nx::network::cloud::tcp {

/**
 * Currently, this class tests that tcp-connection can be established to reported endpoint.
 * No remote peer validation is performed!
 */
class NX_NETWORK_API DirectEndpointConnector:
    public AbstractTunnelConnector
{
    using base_type = AbstractTunnelConnector;

public:
    DirectEndpointConnector(
        AddressEntry targetHostAddress,
        std::string connectSessionId);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual int getPriority() const override;
    virtual void connect(
        const hpm::api::ConnectResponse& response,
        std::chrono::milliseconds timeout,
        ConnectCompletionHandler handler) override;
    virtual const AddressEntry& targetPeerAddress() const override;

    /**
     * Disables verification for test purposes.
     */
    static void setVerificationRequirement(bool value);

protected:
    virtual void stopWhileInAioThread() override;

private:
    struct VerificatorContext
    {
        SocketAddress endpoint;
        std::unique_ptr<AbstractEndpointVerificator> verificator;
        TunnelConnectStatistics stats;
        nx::utils::ElapsedTimer reponseTimer;
    };
    using Verificators = std::list<VerificatorContext>;

    const AddressEntry m_targetHostAddress;
    const std::string m_connectSessionId;
    ConnectCompletionHandler m_completionHandler;
    Verificators m_verificators;
    static bool s_needVerification;

    void performEndpointVerification(
        std::vector<SocketAddress> endpoints,
        std::chrono::milliseconds timeout,
        ConnectCompletionHandler handler);

    void removeInvalidEmptyAddresses(std::vector<SocketAddress>* endpoints);

    void launchVerificators(
        const std::vector<SocketAddress>& endpoints,
        std::chrono::milliseconds timeout);

    void onVerificationDone(
        Verificators::iterator verificatorIter,
        AbstractEndpointVerificator::VerificationResult result);

    void reportErrorOnEndpointVerificationFailure(
        nx::hpm::api::NatTraversalResultCode resultCode,
        SystemError::ErrorCode sysErrorCode,
        VerificatorContext verificatorCtx);

    void reportSuccessfulVerificationResult(VerificatorContext verificatorCtx);
};

} // namespace nx::network::cloud::tcp
