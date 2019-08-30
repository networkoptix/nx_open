#pragma once

#include <nx/network/http/server/abstract_fusion_request_handler.h>
#include <nx/network/cloud/mediator/api/connection_speed.h>

namespace nx::hpm {

class ListeningPeerDb;

class UpdateConnectionSpeedHandler:
    public  nx::network::http::AbstractFusionRequestHandler<api::PeerConnectionSpeed, void>
{
public:
    UpdateConnectionSpeedHandler(ListeningPeerDb* listeningPeerDb);

    virtual void processRequest(
        nx::network::http::RequestContext requestContext,
        api::PeerConnectionSpeed connectionSpeed) override;

    void updateConnectionSpeed(const api::PeerConnectionSpeed& connectionSpeed);

private:
    ListeningPeerDb* m_listeningPeerDb = nullptr;
};

} // namespace nx::hpm