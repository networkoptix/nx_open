#include "mediator_server_connections.h"

namespace nx {
namespace hpm {
namespace api {

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
    // Just in case it's called from own AIO thread without explicit pleaseStop.
    disconnectFromClient();
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
                        NX_LOGX(lm("Check own state has failed: %1").arg(code), cl_logDEBUG1);
                        return client()->closeConnection(SystemError::invalidData);
                    }

                    if (response.state < GetConnectionStateResponse::State::listening)
                    {
                        NX_LOGX(lm("This peer is not listening, state=%1")
                            .arg(response.state), cl_logWARNING);

                        return client()->closeConnection(SystemError::notConnected);
                    }

                    NX_LOGX(lm("Listening state is verified"), cl_logDEBUG2);
                });
        });
}

} // namespace api
} // namespace hpm
} // namespace nx
