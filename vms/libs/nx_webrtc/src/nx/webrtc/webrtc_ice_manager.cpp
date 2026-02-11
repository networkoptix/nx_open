// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_ice_manager.h"

#include "nx_webrtc_ini.h"

#include <common/common_module.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/vms/common/system_settings.h>

namespace nx::webrtc {

static constexpr char kTurnModule[] = "turn";
static constexpr std::chrono::seconds kDiscoveryKeepAliveTimeout{60};

TurnInfoFetcher::TurnInfoFetcher(nx::vms::common::SystemSettings* settings):
    m_systemSettings(settings),
    m_turnInfoFetcher(kTurnModule),
    m_cdbConnection(
        "https://" + nx::network::SocketGlobals::cloud().cloudHost(),
        nx::network::ssl::kDefaultCertificateCheck)
{
}

TurnInfoFetcher::~TurnInfoFetcher()
{
    stop();
}

void TurnInfoFetcher::stop()
{
    m_turnInfoFetcher.pleaseStopSync();
}

void TurnInfoFetcher::onTurnServerInfoUnsafe()
{
    if (!m_turnInfo.address.isNull())
    {
        for (const auto& handlerIter: m_turnInfoSessions)
            handlerIter.second(m_turnInfo);
    }
    m_turnInfoSessions.clear();
}

void TurnInfoFetcher::onOauthInfoUnsafe()
{
    if (m_oauthInfo)
    {
        for (const auto& handlerIter: m_oauthSessions)
            handlerIter.second(*m_oauthInfo);
    }
    m_oauthSessions.clear();
}

void TurnInfoFetcher::fetchTurnInfoUnsafe()
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

void TurnInfoFetcher::fillTurnInfoDefaults()
{
    // Reset previously cached values.
    m_turnInfo = {};
    // Default values.
    static const std::string kTurnRealm = "nx"; //< Doesn't matter, overridden while authorization.
    m_turnInfo.realm = kTurnRealm;
    m_turnInfo.longTermAuth = true;
    // Default values from Cloud.
    m_turnInfo.user = m_systemSettings->cloudSystemId().toStdString();
    m_turnInfo.password = m_systemSettings->cloudAuthKey().toStdString();
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

void TurnInfoFetcher::fetchTurnInfo(const std::string& sessionId, const TurnInfoHandler& handler)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    bool alreadyFetching = !m_turnInfoSessions.empty();
    m_turnInfoSessions[sessionId] = handler;

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

void TurnInfoFetcher::removeTurnInfoHandler(const std::string& sessionId)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_turnInfoSessions.erase(sessionId);
}

void TurnInfoFetcher::removeOauthInfoHandler(const std::string& sessionId)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_oauthSessions.erase(sessionId);
}

void TurnInfoFetcher::fetchThirdPartyAuthorization(
    const std::string& sessionId,
    const std::string& serverName,
    const OauthInfoHandler& handler)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    bool fetching = !m_oauthSessions.empty();
    m_oauthSessions[sessionId] = handler;

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
    m_cdbConnection.setCredentials(
        nx::network::http::PasswordCredentials(
            m_systemSettings->cloudSystemId().toStdString(),
            m_systemSettings->cloudAuthKey().toStdString()));

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
