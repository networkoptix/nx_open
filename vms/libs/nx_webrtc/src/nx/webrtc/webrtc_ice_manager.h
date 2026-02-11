// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <unordered_map>

#include <nx/network/abstract_socket.h>
#include <nx/network/socket_common.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/thread/mutex.h>

// Cloud.
#include <nx/cloud/db/client/cdb_connection.h>
#include <nx/network/cloud/cloud_module_url_fetcher.h>

#include "webrtc_utils.h"

namespace nx::vms::common { class SystemSettings; }

namespace nx::webrtc {

class ConnectionProcessor;

class NX_WEBRTC_API TurnInfoFetcher
{
public:
    using TurnInfoHandler = std::function<void(const TurnServerInfo&)>;
    using OauthInfoHandler = std::function<void(const OauthInfo&)>;

public:
    TurnInfoFetcher(nx::vms::common::SystemSettings* settings);
    ~TurnInfoFetcher();

    void fetchTurnInfo(const std::string& sessionId, const TurnInfoHandler& handler);
    void removeTurnInfoHandler(const std::string& sessionId);

    // API for IceRelay.
    void fetchThirdPartyAuthorization(
        const std::string& sessionId,
        const std::string& serverName,
        const OauthInfoHandler& handler);
    void removeOauthInfoHandler(const std::string& sessionId);

    void stop();

private:
    void getTurnServerInfoUnsafe(const std::string& sessionId);
    void fillTurnInfoDefaults();
    void onTurnServerInfoUnsafe();
    void onOauthInfoUnsafe();
    void fetchTurnInfoUnsafe();

private:
    mutable nx::Mutex m_mutex;

    nx::vms::common::SystemSettings* const m_systemSettings;

    // Cloud-related.
    TurnServerInfo m_turnInfo;
    std::optional<OauthInfo> m_oauthInfo;
    nx::utils::ElapsedTimer m_turnInfoTimer;
    std::unordered_map<std::string, TurnInfoHandler> m_turnInfoSessions;
    std::unordered_map<std::string, OauthInfoHandler> m_oauthSessions;
    nx::network::cloud::CloudModuleUrlFetcher m_turnInfoFetcher;

    // Should be destroyed first to ensure all handlers are stopped before
    // other memebers destruction.
    nx::cloud::db::client::Connection m_cdbConnection;
};

} // namespace nx::webrtc
