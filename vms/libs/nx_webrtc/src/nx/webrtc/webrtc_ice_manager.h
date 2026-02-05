// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <unordered_map>

#include <nx/network/abstract_socket.h>
#include <nx/network/socket_common.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/thread/mutex.h>

#include <nx/vms/common/system_context_aware.h>

// Cloud.
#include <nx/cloud/db/client/cdb_connection.h>
#include <nx/network/cloud/cloud_module_url_fetcher.h>

#include "webrtc_utils.h"

namespace nx::webrtc {

class IceTcp;
class IceUdp;
class IceRelay;
class Srflx;
class Session;
class ConnectionProcessor;
class SessionPool;

class NX_WEBRTC_API IceManager: public nx::vms::common::SystemContextAware
{
public:
    IceManager(
        nx::vms::common::SystemContext* systemContext,
        SessionPool* sessionPool);
    ~IceManager();
    // API for SessionPool.
    std::vector<IceCandidate> allocateIce(
        const std::string& sessionId,
        const SessionConfig& config);
    void allocateIceAsync(
        const std::string& sessionId,
        const SessionConfig& config);
    void freeIce(const std::string& sessionId);
    // API for Tracker.
    void gotIceFromTracker(const Session* session, const IceCandidate& iceCandidate);
    // API for IceTcp.
    void gotIceTcp(std::unique_ptr<IceTcp>&& iceTcp);
    void removeIceTcp(IceTcp* iceTcp);
    // API for Srflx.
    void gotSrflxResult(
        Srflx* srflx,
        const std::string& sessionId,
        const SessionConfig& config,
        const nx::network::SocketAddress& mappedAddress,
        std::unique_ptr<nx::network::AbstractDatagramSocket>&& socket);
    // API for IceRelay.
    void getThirdPartyAuthorization(const std::string& sessionId, const std::string& serverName);
    void stop();

    void setDefaultStunServers(std::vector<nx::network::SocketAddress> defaultStunServers);

private:
    std::vector<nx::network::SocketAddress> getStunServers();
    void getTurnServerInfoUnsafe(const std::string& sessionId);
    void fillTurnInfoDefaults();
    void onTurnServerInfoUnsafe();
    void onOauthInfoUnsafe();
    void fetchTurnInfoUnsafe();
private:
    std::map<std::string, std::vector<std::unique_ptr<IceUdp>>> m_sessionToUdpIce;
    std::map<std::string, std::unique_ptr<IceUdp>> m_sessionToPunchedUdpIce;
    std::map<std::string, std::unique_ptr<IceRelay>> m_sessionToRelayedIce;
    std::map<std::string, std::pair<IceCandidate, SessionDescription>> m_sessionToRemoteSrflx;
    std::map<IceTcp*, std::unique_ptr<IceTcp>> m_tcpIces; //< NOTE: self-destructing objects.
    std::map<Srflx*, std::unique_ptr<Srflx>> m_srflx; //< NOTE: self-destructing objects.
    mutable nx::Mutex m_mutex;
    // Cloud-related.
    TurnServerInfo m_turnInfo;
    std::optional<OauthInfo> m_oauthInfo;
    nx::utils::ElapsedTimer m_turnInfoTimer;
    std::set<std::string> m_turnInfoSessions;
    std::set<std::string> m_oauthSessions;
    nx::network::cloud::CloudModuleUrlFetcher m_turnInfoFetcher;

    // Should be destroyed first to ensure all handlers are stopped before
    // other memebers destruction.
    nx::cloud::db::client::Connection m_cdbConnection;
    SessionPool* m_sessionPool;
    std::vector<nx::network::SocketAddress> m_defaultStunServers;
};

} // namespace nx::webrtc
