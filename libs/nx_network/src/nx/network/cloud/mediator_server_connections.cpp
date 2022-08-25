// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mediator_server_connections.h"

namespace nx::hpm::api {

MediatorServerTcpConnection::MediatorServerTcpConnection(
    std::shared_ptr<nx::network::stun::AbstractAsyncClient> stunClient,
    AbstractCloudSystemCredentialsProvider* connector)
:
    MediatorServerConnection<network::stun::AsyncClientUser>(
        std::move(stunClient), connector)
{
}

MediatorServerTcpConnection::~MediatorServerTcpConnection()
{
    // Need to stop ancestor class aio::Timer before destuction of this class to stop any timer
    // callbacks from accessing this object after it destruction
    // (or any parents between here and aio::Timer).
    pleaseStopSync();
}

void MediatorServerTcpConnection::setOnConnectionRequestedHandler(
    std::function<void(nx::hpm::api::ConnectionRequestedEvent)> handler)
{
    setIndicationHandler(
        nx::network::stun::extension::indications::connectionRequested,
        [handler = std::move(handler)](nx::network::stun::Message msg)
        {
            ConnectionRequestedEvent indicationData;
            indicationData.parse(msg);
            handler(std::move(indicationData));
        });
}

void MediatorServerTcpConnection::monitorListeningState(
    std::chrono::milliseconds repeatPeriod)
{
    setConnectionTimer(
        repeatPeriod,
        [this]()
        {
            getConnectionState(
                [this](ResultCode code, GetConnectionStateResponse response)
                {
                    if (code != ResultCode::ok)
                    {
                        NX_DEBUG(this, "Check own state has failed: %1", toString(code));
                        return client()->closeConnection(SystemError::invalidData);
                    }

                    if (response.state < GetConnectionStateResponse::State::listening)
                    {
                        NX_WARNING(this, nx::format("This peer is not listening, state=%1")
                            .arg(response.state));

                        return client()->closeConnection(SystemError::notConnected);
                    }

                    NX_VERBOSE(this, nx::format("Listening state is verified"));
                });
        });
}

} // namespace nx::hpm::api
