// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_ice_manager.h"

#include "nx_webrtc_ini.h"

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/system_context.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/vms/common/system_settings.h>

#include "webrtc_ice_relay.h"
#include "webrtc_ice_tcp.h"
#include "webrtc_ice_udp.h"
#include "webrtc_session.h"
#include "webrtc_session_pool.h"
#include "webrtc_srflx.h"

namespace nx::webrtc {

static constexpr char kTurnModule[] = "turn";
static constexpr std::chrono::seconds kDiscoveryKeepAliveTimeout{60};

IceManager::IceManager(
    nx::vms::common::SystemContext* systemContext,
    webrtc::SessionPool* sessionPool)
    :
    nx::vms::common::SystemContextAware(systemContext),
    m_turnInfoFetcher(kTurnModule),
    m_cdbConnection(
        "https://" + nx::network::SocketGlobals::cloud().cloudHost(),
        nx::network::ssl::kDefaultCertificateCheck),
    m_sessionPool(sessionPool)
{
}

IceManager::~IceManager()
{
    stop();
}

void IceManager::stop()
{
    decltype(m_tcpIces) icesTcp;
    decltype(m_sessionToRelayedIce) icesRelay;
    decltype(m_srflx) srflx;
    decltype(m_sessionToPunchedUdpIce) icesPunchedUdp;
    decltype(m_sessionToUdpIce) icesUdp;
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        std::swap(icesTcp, m_tcpIces);
        std::swap(icesRelay, m_sessionToRelayedIce);
        std::swap(srflx, m_srflx);
        std::swap(icesPunchedUdp, m_sessionToPunchedUdpIce);
        std::swap(icesUdp, m_sessionToUdpIce);
    }
    icesTcp.clear();
    icesRelay.clear();
    srflx.clear();
    icesPunchedUdp.clear();
    icesUdp.clear();
    m_turnInfoFetcher.pleaseStopSync();
}

void IceManager::setDefaultStunServers(std::vector<nx::network::SocketAddress> defaultStunServers)
{
    m_defaultStunServers = std::move(defaultStunServers);
}

std::vector<nx::network::SocketAddress> IceManager::getStunServers()
{

    auto connectionMediatorAddress = SocketGlobals::cloud().mediatorConnector().address();

    std::vector<nx::network::SocketAddress> result;
    if (connectionMediatorAddress)
        result.push_back(connectionMediatorAddress->stunUdpEndpoint);
    result.insert(result.end(), m_defaultStunServers.begin(), m_defaultStunServers.end());

    return result;
}

void IceManager::onTurnServerInfoUnsafe()
{
    if (!m_turnInfo.address.isNull())
    {
        for (const auto& sessionId: m_turnInfoSessions)
        {
            if (auto relay = m_sessionToRelayedIce.find(sessionId);
                relay != m_sessionToRelayedIce.end())
            {
                relay->second->startRelay(m_turnInfo);
            }
        }
    }
    m_turnInfoSessions.clear();
}

void IceManager::onOauthInfoUnsafe()
{
    if (m_oauthInfo)
    {
        for (const auto& sessionId: m_oauthSessions)
        {
            if (auto relay = m_sessionToRelayedIce.find(sessionId);
                relay != m_sessionToRelayedIce.end())
            {
                relay->second->setOauthInfo(*m_oauthInfo);
            }
        }
    }
    m_oauthSessions.clear();
}

void IceManager::fetchTurnInfoUnsafe()
{
    m_turnInfoFetcher.setModulesXmlUrl(
        nx::network::AppInfo::defaultCloudModulesXmlUrl(
            nx::network::SocketGlobals::cloud().cloudHost()));
    m_turnInfoFetcher.get(
        [this](nx::network::http::StatusCode::Value code, nx::Url url)
        {
            // Sometimes it called immediately with cached result.
            m_turnInfoFetcher.post(
                [this, code, url]
                {
                    NX_MUTEX_LOCKER lk(&m_mutex);
                    NX_DEBUG(this, "Discovery: got code %1, url %2", code, url);
                    if (nx::network::http::StatusCode::isSuccessCode(code) && !url.isEmpty())
                        m_turnInfo.address = nx::network::SocketAddress::fromUrl(url);

                    onTurnServerInfoUnsafe();
                    m_turnInfoTimer.restart();
                });
        });
}

void IceManager::fillTurnInfoDefaults()
{
    // Reset previously cached values.
    m_turnInfo = {};
    // Default values.
    static const std::string kTurnRealm = "nx"; //< Doesn't matter, overridden while authorization.
    m_turnInfo.realm = kTurnRealm;
    m_turnInfo.longTermAuth = true;
    // Default values from Cloud.
    m_turnInfo.user = globalSettings()->cloudSystemId().toStdString();
    m_turnInfo.password = globalSettings()->cloudAuthKey().toStdString();
    // Overriding values from ini().
    const std::string turnServerIni = ini().turnServer;
    auto atPos = turnServerIni.rfind('@');
    if (atPos != std::string::npos)
    {
        m_turnInfo.address = nx::network::SocketAddress(turnServerIni.substr(atPos + 1));
        auto colonPos = turnServerIni.find(':');
        if (colonPos != std::string::npos && colonPos < atPos)
        {
            m_turnInfo.user = turnServerIni.substr(0, colonPos);
            m_turnInfo.password = turnServerIni.substr(colonPos + 1, atPos - colonPos - 1);
        }
    }
    else if (!turnServerIni.empty())
    {
        m_turnInfo.address = nx::network::SocketAddress(turnServerIni);
    }
}

void IceManager::getTurnServerInfoUnsafe(const std::string& sessionId)
{
    bool alreadyFetching = !m_turnInfoSessions.empty();
    m_turnInfoSessions.insert(sessionId);

    // Check if already requested nearest TURN server.
    if (alreadyFetching)
        return;

    if (m_turnInfoTimer.isValid() && !m_turnInfoTimer.hasExpired(kDiscoveryKeepAliveTimeout))
    {
        // Has actual TURN server information stored.
        onTurnServerInfoUnsafe();
        return;
    }
    // Fill turnInfo from config.
    fillTurnInfoDefaults();
    // Reset previously cached value.
    m_turnInfoFetcher.resetUrl();
    // Request Discovery service if not overridden in ini().
    if (m_turnInfo.address.isNull() && !nx::network::SocketGlobals::cloud().cloudHost().empty())
    {
        fetchTurnInfoUnsafe();
    }
    else
    {
        onTurnServerInfoUnsafe();
        m_turnInfoTimer.restart();
    }
}

std::vector<IceCandidate> IceManager::allocateIce(
    const std::string& sessionId,
    const SessionConfig& config)
{
    int priority = IceCandidate::kIceMaxPriority;
    int index = 0;
    std::vector<IceCandidate> result;
    std::vector<std::unique_ptr<IceUdp>> ices;

    auto server = systemContext()->resourcePool()->getResourceById<QnMediaServerResource>(
        systemContext()->peerId());
    if (!server)
        return result;
    auto allAddresses = server->getAllAvailableAddresses();

    // TCP host candidates.
    for (const auto& address: allAddresses)
    {
        result.push_back(
            IceCandidate::constructTcpHost(index++, priority--, address));
    }

    // UDP host candidates.
    for (const auto& address: allAddresses)
    {
        auto udpIce = std::make_unique<IceUdp>(m_sessionPool, config);
        auto iceLocalAddress = udpIce->bind(address.address);
        if (iceLocalAddress)
        {
            result.push_back(
                IceCandidate::constructUdpHost(
                    index++,
                    priority--,
                    *iceLocalAddress));
            ices.push_back(std::move(udpIce));
        }
    }

    NX_MUTEX_LOCKER lk(&m_mutex);
    NX_ASSERT(m_sessionToUdpIce.count(sessionId) == 0);
    m_sessionToUdpIce[sessionId] = std::move(ices);
    return result;
}

void IceManager::allocateIceAsync(
    const std::string& sessionId,
    const SessionConfig& config)
{
    // Srflx candidate.
    // Use first 2 accessible STUN servers from list.
    {
        Srflx* srflxPtr;
        {
            auto srflx = std::make_unique<Srflx>(
                m_sessionPool, getStunServers(), sessionId, config);
            NX_MUTEX_LOCKER lk(&m_mutex);
            srflxPtr = srflx.get();
            m_srflx[srflxPtr] = std::move(srflx);
        }
        srflxPtr->start(); // srflxPtr can be deleted while start is in processing.
    }

    // Relay candidate
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_sessionToRelayedIce[sessionId] = std::make_unique<IceRelay>(
        m_sessionPool,
        config,
        sessionId);
    getTurnServerInfoUnsafe(sessionId);
}

template <typename T>
T getMappedIce(std::map<std::string, T>& m, const std::string& sessionId)
{
    T ice;
    if (auto it = m.find(sessionId); it != m.end())
    {
        ice = std::move(it->second);
        m.erase(it);
    }
    return ice;
}

void IceManager::freeIce(const std::string& sessionId)
{
    decltype(m_sessionToUdpIce)::mapped_type iceUdp;
    decltype(m_sessionToPunchedUdpIce)::mapped_type iceUdpPunched;
    decltype(m_sessionToRelayedIce)::mapped_type iceRelay;

    {
        NX_MUTEX_LOCKER lk(&m_mutex);

        // Objects with non-trivial destruction.
        iceUdp = getMappedIce(m_sessionToUdpIce, sessionId);
        iceUdpPunched = getMappedIce(m_sessionToPunchedUdpIce, sessionId);
        iceRelay = getMappedIce(m_sessionToRelayedIce, sessionId);

        // A simple objects.
        m_sessionToRemoteSrflx.erase(sessionId);
        m_turnInfoSessions.erase(sessionId);
        m_oauthSessions.erase(sessionId);
    }
}

void IceManager::gotIceFromTracker(const Session* session, const IceCandidate& iceCandidate)
{
    if (iceCandidate.type != IceCandidate::Type::Srflx)
        return;

    NX_CRITICAL(session != nullptr);
    NX_DEBUG(this, "Got ice srflx from peer: %1 -> %2", session->id(), iceCandidate.serialize());

    NX_MUTEX_LOCKER lk(&m_mutex);
    auto udpIce = m_sessionToPunchedUdpIce.find(session->id());
    if (udpIce != m_sessionToPunchedUdpIce.end())
        udpIce->second->sendBinding(iceCandidate, session->describe());
    else
        m_sessionToRemoteSrflx[session->id()] = {iceCandidate, session->describe()};
    auto relayIce = m_sessionToRelayedIce.find(session->id());
    if (relayIce != m_sessionToRelayedIce.end())
        relayIce->second->setRemoteSrflx(iceCandidate);
}

void IceManager::gotSrflxResult(
    Srflx* srflx,
    const std::string& sessionId,
    const SessionConfig& config,
    const nx::network::SocketAddress& mappedAddress,
    std::unique_ptr<nx::network::AbstractDatagramSocket>&& socket)
{
    NX_MUTEX_LOCKER lk(&m_mutex);

    if (!m_srflx.count(srflx))
        return; //< Possible called while server is stopping.

    // 1. Create IceUdp with given socket.
    std::unique_ptr<IceUdp> punchedUdpIce;
    nx::network::SocketAddress iceLocalAddress;
    if (socket && !mappedAddress.isNull())
    {
        iceLocalAddress = socket->getLocalAddress();
        punchedUdpIce = std::make_unique<IceUdp>(m_sessionPool, config);
        punchedUdpIce->resetSocket(std::move(socket));
    }
    // 2. Report Session about candidate.
    if (!mappedAddress.isNull())
    {
        IceCandidate srflxCandidate = IceCandidate::constructUdpSrflx(
            IceCandidate::kIceMinIndex,
            IceCandidate::kIceMaxPriority,
            mappedAddress,
            iceLocalAddress);
        m_sessionPool->gotIceFromManager(
            sessionId, std::move(srflxCandidate));
    }

    // 3. Store IceUdp.
    if (NX_ASSERT(m_sessionToPunchedUdpIce.count(sessionId) == 0) && punchedUdpIce)
    {
        if (const auto remoteSrflx = m_sessionToRemoteSrflx.find(sessionId);
            remoteSrflx != m_sessionToRemoteSrflx.end())
        {
            punchedUdpIce->sendBinding(remoteSrflx->second.first, remoteSrflx->second.second);
        }
        m_sessionToPunchedUdpIce[sessionId] = std::move(punchedUdpIce);
    }

    // 4. Remove Srflx.
    m_srflx.erase(srflx);
}

void IceManager::gotIceTcp(std::unique_ptr<IceTcp>&& iceTcp)
{
    auto iceTcpPtr = iceTcp.get();
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        m_tcpIces.emplace(iceTcpPtr, std::move(iceTcp));
    }
    iceTcpPtr->start();
}

void IceManager::removeIceTcp(IceTcp* iceTcp)
{
    std::unique_ptr<IceTcp> iceTcpPtr; //< Delete outside of guarding to avoid deadlock.
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        auto iter = m_tcpIces.find(iceTcp);
        if (iter == m_tcpIces.end())
            return;
        iceTcpPtr = std::move(iter->second);
        m_tcpIces.erase(iceTcp);
    }
}

void IceManager::getThirdPartyAuthorization(const std::string& sessionId, const std::string& serverName)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    bool fetching = !m_oauthSessions.empty();
    m_oauthSessions.insert(sessionId);

    if (fetching)
        return;

    if (m_oauthInfo && m_oauthInfo->expires_at >
        std::chrono::duration_cast<std::chrono::seconds>(nx::utils::utcTime().time_since_epoch()))
    {
        onOauthInfoUnsafe();
        return;
    }

    if (nx::network::SocketGlobals::cloud().cloudHost().empty())
    {
        onOauthInfoUnsafe();
        return;
    }
    const auto settings = globalSettings();
    m_cdbConnection.setCredentials(
        nx::network::http::PasswordCredentials(
            settings->cloudSystemId().toStdString(), settings->cloudAuthKey().toStdString()));

    m_cdbConnection.oauthManager()->issueStunToken(
        nx::cloud::db::api::IssueStunTokenRequest{.server_name = serverName},
        [this](auto code, auto response)
        {
            // Always being posted.
            NX_MUTEX_LOCKER lk(&m_mutex);
            if (code != nx::cloud::db::api::ResultCode::ok)
            {
                NX_DEBUG(this, "Oauth: unexpected error code: %1", code);
                m_oauthInfo = std::nullopt;
            }
            else if (response.error)
            {
                NX_DEBUG(this, "Oauth: got error while generating token: %1", *response.error);
                m_oauthInfo = std::nullopt;
            }
            else
            {
                constexpr auto kOauthExpiratinGap = std::chrono::seconds(5);
                auto expiresAt =
                    std::chrono::duration_cast<std::chrono::seconds>(
                        nx::utils::utcTime().time_since_epoch())
                    + response.expires_in
                    - (response.expires_in > kOauthExpiratinGap
                        ? kOauthExpiratinGap
                        : std::chrono::seconds(0));
                m_oauthInfo =
                    OauthInfo{
                        .kid = response.kid,
                        .lifetime = static_cast<int>(response.expires_in.count()), // Actually unused.
                        .expires_at = expiresAt,
                        .sessionKey = nx::utils::fromBase64(response.mac_code),
                        .accessToken = nx::utils::fromBase64(response.token)
                    };

                NX_DEBUG(this, "Oauth: got token with kid %1 and lifetime %2",
                    m_oauthInfo->kid,
                    m_oauthInfo->lifetime);
            }
            onOauthInfoUnsafe();
        });
    // turnutils_oauth -e --server-name blackdow.carleon.gov --auth-key-id oldempire \
    // --auth-key MTIzNDU2Nzg5MDEyMzQ1Njc4OTA= --auth-key-timestamp 1664203748 --auth-key-lifetime 21600 \
    // --token-mac-key MTIzNDU2Nzg5MDEyMzQ1Njc4OTA=  --token-timestamp `expr $(date +%s) \* 65536` \
    // --token-lifetime=3600 --auth-key-as-rs-alg A256GCM
    // , where:
    // server-name - authorization server name.
    // auth-key-id - key id, stored in redis db as part of key path: 'turn/oauth/kid/oldempire'.
    // auth-key - key which also should be stored in redis db:
    //     hmset turn/oauth/kid/oldempire ikm_key MTIzNDU2Nzg5MDEyMzQ1Njc4OTAx
    //  auth-key-timestamp - unix time of key emitted.
    //  auth-key-lifetime - key lifetime. Probably doesn't checked in authorization process.
    //  token-mac-key - temporary token key. Should be 20 bytes length sha1 of some secret key.
    //  token-timestamp - timestamp of token emit.
    //  token-lifetime - lifetime. Checked in authorization process.
    //  auth-key-as-rs-alg - some algorithm, should be same in redis db:
    //      hmset turn/oauth/kid/oldempire as_rs_alg A256GCM
}

} // namespace nx::webrtc
