// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_session.h"

#include <core/resource/camera_resource.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/utils/crypt/linux_passwd_crypt.h>
#include <nx/vms/common/system_context.h>

#include "webrtc_ice_relay.h"
#include "webrtc_ice_tcp.h"
#include "webrtc_ice_udp.h"
#include "webrtc_receiver.h"
#include "webrtc_streamer.h"
#include "webrtc_tracker.h"
#include "webrtc_session_pool.h"
#include "webrtc_srflx.h"
#include "webrtc_tracker.h"

/* RFC list:
 * https://webrtcforthecurious.com/docs/03-connecting/ - Overview of connecting process
 * http://rtcweb-wg.github.io/jsep/ - JavaScript Session Establishment Protocol
 * https://datatracker.ietf.org/doc/html/rfc3264 - An Offer/Answer Model with the Session Description Protocol (SDP)
 * https://datatracker.ietf.org/doc/html/rfc5245 - Interactive Connectivity Establishment (ICE) - old.
 * https://datatracker.ietf.org/doc/html/rfc8445 - Interactive Connectivity Establishment (ICE)
 * https://datatracker.ietf.org/doc/html/rfc6544 - TCP Candidates with Interactive Connectivity Establishment (ICE)
 * https://datatracker.ietf.org/doc/html/rfc4145 - TCP-Based Media Transport in the Session Description Protocol (SDP)
 * https://datatracker.ietf.org/doc/html/rfc4571 - Framing RTP and RTCP over Connection-Oriented Transport
 * https://datatracker.ietf.org/doc/html/rfc8489 - Session Traversal Utilities for NAT (STUN)
 * https://datatracker.ietf.org/doc/html/rfc8656 - Traversal Using Relays around NAT (TURN)
 * https://datatracker.ietf.org/doc/html/rfc5764 - DTLS Extension to Establish Keys for the SRTP
 * https://datatracker.ietf.org/doc/html/rfc3711 - The Secure Real-time Transport Protocol (SRTP)
 * https://datatracker.ietf.org/doc/html/rfc4585 - Extended RTP Profile for RTCP-Based Feedback (RTP/AVPF)
 * */

namespace {

std::string generatePwd(int size)
{
    auto result = nx::crypt::generateSalt(size).toStdString();
    std::for_each(result.begin(), result.end(),
        [](char& c)
        {
            if (c == '.')
            c = '+';
        });
    return result;
}

static const AVCodecID kFallbackSrtpVideoCodec = AV_CODEC_ID_VP8;
static const AVCodecID kFallbackSrtpAudioCodec = AV_CODEC_ID_PCM_MULAW;

static const AVCodecID kFallbackMseVideoCodec = AV_CODEC_ID_H264;
static const AVCodecID kFallbackMseAudioCodec = AV_CODEC_ID_AAC;

}

namespace nx::webrtc {

using namespace nx::network;

std::string Session::generateLocalUfrag()
{
    return generatePwd(4);
}

Session::Session(
    nx::vms::common::SystemContext* context,
    SessionPool* sessionPool,
    createDataProviderFactory providerFactory,
    const std::string& localUfrag,
    const nx::network::SocketAddress& address,
    const QList<nx::network::SocketAddress>& allAddresses,
    const SessionConfig& config,
    const std::vector<nx::network::SocketAddress>& stunServers,
    Purpose purpose)
    :
    m_SystemContext(context),
    m_purpose(purpose),
    m_localAddress(address),
    m_pem(ssl::Context::instance()->getDefaultCertificate()),
    m_tracker(std::make_shared<Tracker>(this)),
    m_stunServers(stunServers),
    m_sessionPool(sessionPool),
    m_config(config)
{
    createIces(allAddresses);
    m_localUfrag = localUfrag;
    m_localPwd = generatePwd(24);
    m_dtls = std::make_shared<Dtls>(m_localUfrag, m_pem);

    m_tracks = std::make_unique<TracksForSend>(this);
    m_transceiver = std::make_unique<Streamer>(this);

    if (m_purpose == Purpose::send)
    {
        m_consumer = std::make_unique<Consumer>(this);
        m_providerFactory = std::move(providerFactory);
    }
    else if (m_purpose == Purpose::recv)
    {
        auto video = std::make_unique<Track>(Track{
            .mid = 0,
            .payloadType = 96,
            .trackType = TrackType::video,
            });
        m_tracks->addTrack(std::move(video));
        auto audio = std::make_unique<Track>(Track{
            .mid = 1,
            .payloadType = 0,
            .trackType = TrackType::audio,
        });
        m_tracks->addTrack(std::move(audio));

        m_demuxer = std::make_unique<Demuxer>(m_tracks.get());
    }
}

Session::~Session()
{
    m_sessionPool->turnInfoFetcher().removeOauthInfoHandler(id());
    m_reader.reset();
    releaseTracker(); //< Should be guarded.
}

void Session::createIces(const QList<nx::network::SocketAddress>& addresses)
{
    int priority = IceCandidate::kIceMaxPriority;
    int index = 0;
    // TCP host candidates.
    for (const auto& address: addresses)
    {
        m_iceCandidates.push_back(
            IceCandidate::constructTcpHost(index++, priority--, address));
    }

    // UDP host candidates.
    for (const auto& address: addresses)
    {
        auto udpIce = std::make_unique<IceUdp>(m_sessionPool, m_config);
        auto iceLocalAddress = udpIce->bind(address.address);
        if (iceLocalAddress)
        {
            m_iceCandidates.push_back(
                IceCandidate::constructUdpHost(
                    index++,
                    priority--,
                    *iceLocalAddress));
            m_udpIces.push_back(std::move(udpIce));
        }
    }

    // Srflx candidate.
    // Use first 2 accessible STUN servers from list.
    m_srflx = std::make_unique<Srflx>(m_localUfrag, m_stunServers,
        [this](const nx::network::SocketAddress& mappedAddress,
            std::unique_ptr<nx::network::AbstractDatagramSocket>&& socket)
        {
            NX_DEBUG(this, "%1: Got Srflx remote addres: %2, local address: %3",
                id(),
                mappedAddress.toString(),
                socket ? socket->getLocalAddress().toString() : "null");
            // 1. Create IceUdp with given socket.
            nx::network::SocketAddress iceLocalAddress;
            if (socket && !mappedAddress.isNull())
            {
                iceLocalAddress = socket->getLocalAddress();

                NX_MUTEX_LOCKER lk(&m_mutex);
                m_punchedUdpIce = std::make_unique<IceUdp>(m_sessionPool, m_config);
                m_punchedUdpIce->resetSocket(std::move(socket));
                if (m_remoteSrflx)
                    m_punchedUdpIce->sendBinding(*m_remoteSrflx.get(), describeUnsafe());
            }

            // 2. Add candidate to list.
            if (!mappedAddress.isNull())
            {
                IceCandidate srflxCandidate = IceCandidate::constructUdpSrflx(
                    IceCandidate::kIceMinIndex,
                    IceCandidate::kIceMaxPriority,
                    mappedAddress,
                    iceLocalAddress);
                addIceCandidate(std::move(srflxCandidate));
            }
        });

    m_relayIce = std::make_unique<IceRelay>(m_sessionPool, m_config, id(),
        [this](const IceCandidate& candidate)
        {
            addIceCandidate(candidate);
        });

    m_sessionPool->turnInfoFetcher().fetchTurnInfo(
        id(),
        [this](const TurnServerInfo& info) {
            m_relayIce->startRelay(info);
        });
}

void Session::gotIceFromTracker(const IceCandidate& iceCandidate)
{
    NX_DEBUG(this, "Got ice srflx from peer: %1 -> %2", id(), iceCandidate.serialize());

    NX_MUTEX_LOCKER lk(&m_mutex);
    if (m_punchedUdpIce)
        m_punchedUdpIce->sendBinding(iceCandidate, describeUnsafe());
    m_remoteSrflx = std::make_unique<IceCandidate>(iceCandidate);
    m_relayIce->setRemoteSrflx(iceCandidate);
}

AbstractCameraDataProviderPtr Session::createProvider(
    nx::Uuid deviceId,
    std::optional<std::chrono::milliseconds> positionMs,
    nx::vms::api::StreamIndex stream,
    std::optional<float> speedOpt)
{
    auto provider = m_providerFactory(deviceId, positionMs, stream, speedOpt);
    addProvider(provider);

    if (!initializeMuxersInternal())
        return AbstractCameraDataProviderPtr();

    if (m_tracker->isStarted())
        m_tracker->sendOffer();

    return provider;
}

void Session::addProvider(std::shared_ptr<AbstractCameraDataProvider> provider)
{
    const auto deviceId = provider->resource()->getId();

    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_stopped)
        return;

    if (provider->getVideoCodecParameters())
    {
        auto video = std::make_unique<Track>(Track{
            .mid = 0,
            .deviceId = deviceId,
            .trackType = TrackType::video,
            });
        m_tracks->addTrack(std::move(video));
    }

    if (provider->getAudioCodecParameters())
    {
        auto audio = std::make_unique<Track>(Track{
            .mid = 1,
            .deviceId = deviceId,
            .trackType = TrackType::audio,
            });
        m_tracks->addTrack(std::move(audio));
    }

    m_providers.emplace(deviceId, provider);
    provider->setDataConsumer(m_consumer.get());
}

void Session::setTranscodingParams(
    nx::vms::api::WebRtcMethod method,
    QnLegacyTranscodingSettings transcodingSettings,
    std::optional<nx::vms::api::ResolutionData> resolution,
    std::optional<nx::vms::api::ResolutionData> resolutionWhenTranscoding,
    nx::vms::api::WebRtcTrackerSettings::MseFormat mseFormat)
{
    m_method = method;
    m_transcodingSettings = transcodingSettings;
    m_resolution = resolution;
    m_resolutionWhenTranscoding = resolutionWhenTranscoding;
    m_mseFormat = mseFormat;
    m_muxer = std::make_unique<Transcoder>(this, m_method, m_mseFormat);
}

bool Session::initializeMuxersInternal()
{
    // Ignore aspect ratio if user specifies the full resolution.
    if (m_resolution && m_resolution->size.width() > 0)
        m_transcodingSettings.forcedAspectRatio = QnAspectRatio();

    for (const auto& [_, provider]: m_providers)
    {
        if (provider->isInitialized())
            continue;

        const auto videoTrack = m_tracks->videoTrack(provider->resource()->getId());
        const auto audioTrack = m_tracks->audioTrack(provider->resource()->getId());

        const bool useFallbackVideo = videoTrack && videoTrack->state == TrackState::inactive;
        const bool useFallbackAudio = audioTrack && audioTrack->state == TrackState::inactive;

        auto videoCodecParamaters = provider->getVideoCodecParameters();
        if (videoCodecParamaters)
        {
            bool videoTranscodingRequired = !m_transcodingSettings.isEmpty()
                || m_resolution
                || useFallbackVideo
                || !m_muxer->isVideoCodecSupported(videoCodecParamaters->codec_id);

            const auto transcodePolicy = m_sessionPool->transcodePolicy();
            if (transcodePolicy.disableTranscoding && videoTranscodingRequired)
            {
                NX_DEBUG(this,
                    "%1: Video transcoding is disabled(source codec: %2) in the server settings. "
                    "Feature unavailable. RTP",
                    id(),
                    videoCodecParamaters->codec_id);
                return false;
            }

            if (videoTranscodingRequired)
            {
                QnFfmpegVideoTranscoder::Config config(transcodePolicy);
                config.targetCodecId = m_method == nx::vms::api::WebRtcMethod::srtp ?
                    kFallbackSrtpVideoCodec : kFallbackMseVideoCodec;
                config.quality = Qn::StreamQuality::normal;
                if (m_method == nx::vms::api::WebRtcMethod::srtp)
                    config.rtpContatiner = true;

                if (m_resolution)
                    config.outputResolutionLimit = m_resolution->size;
                else if (m_resolutionWhenTranscoding)
                    config.outputResolutionLimit = m_resolutionWhenTranscoding->size;
                config.useMultiThreadEncode = nx::transcoding::useMultiThreadEncode(
                    config.targetCodecId, config.outputResolutionLimit);

                config.params =
                    suggestMediaStreamParams(config.targetCodecId, config.quality);
                if (m_method == nx::vms::api::WebRtcMethod::mse)
                {
                    // Gop size should be less then MAX_FRAME_DURATION (5s) for correct delay
                    // calculation in Consumer::calculateDelay(). You can only specify gop_size in
                    // number of frames, so 20/ is enough for fps >= 5.
                    config.params.insert(QnCodecParams::gop_size, 20);
                }
                if (!provider->addVideoTranscoder(config, m_transcodingSettings))
                    return false;

                NX_DEBUG(this, "%1: Video transcoding started, target codec: %2",
                    id(), config.targetCodecId);
            }
        }

        auto audioCodecParamaters = provider->getAudioCodecParameters();
        if (audioCodecParamaters)
        {
            bool audioTranscodingRequired = useFallbackAudio
                || !m_muxer->isAudioCodecSupported(audioCodecParamaters->codec_id);
            if (audioTranscodingRequired)
            {
                auto audioCodecId = m_method == nx::vms::api::WebRtcMethod::srtp
                    ? kFallbackSrtpAudioCodec : kFallbackMseAudioCodec;
                if (!provider->addAudioTranscoder(audioCodecId))
                    return false;

                NX_DEBUG(this, "%1: Audio transcoding started, target codec: %2",
                    id(), audioCodecId);
            }
        }

        if (!m_muxer->createEncoders(
            provider->resource()->getId(),
            provider->getVideoCodecParameters(),
            provider->getAudioCodecParameters()))
        {
            return false;
        }
        provider->setInitialized(true);
    }
    return true;
}

void Session::setReader(std::shared_ptr<Reader> reader)
{
    m_reader = std::move(reader);
}

void Session::setAddress(const nx::network::SocketAddress& address)
{
    m_localAddress = address;
}

std::shared_ptr<nx::metric::Storage> Session::metrics() const
{
    return m_SystemContext->metrics();
}

std::optional<SessionDescription> Session::describe() const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    if (isLocked())
        return std::nullopt;

    return describeUnsafe();
}

SessionDescription Session::describeUnsafe() const
{
    return SessionDescription
    {
        .id = m_localUfrag,
        .remoteUfrag = m_sdpParseResult.iceUfrag,
        .localPwd = m_localPwd,
        .dtls = m_dtls
    };
}

std::string Session::dataChannelSdp() const
{
#define ENDL "\r\n";
    std::string sdp;
    sdp += "m=application 0 UDP/DTLS/SCTP webrtc-datachannel" ENDL;
    sdp += "a=mid:" + std::to_string(kDataChannelMid) + ENDL;
    // Port in SCTP level.
    sdp += "a=sctp-port:" + std::to_string(kDefaultSctpPort) + ENDL;
    // SCTP max message size.
    sdp += "a=max-message-size:" + std::to_string(kSctpMaxMessageSize) + ENDL;
    // Can be only bundled with another tracks.
    sdp += "a=bundle-only" ENDL;
    /* https://datatracker.ietf.org/doc/html/rfc3264#section-5.1
     * If the offerer wishes to only send media on a stream to its peer, it
     * MUST mark the stream as sendonly with the "a=sendonly" attribute.
     * */
    sdp += "a=sendrecv" ENDL;
    /* 'actpass' due to bug in old Chromium. Actually, for incoming connection, should be 'passive':
     * https://datatracker.ietf.org/doc/html/rfc4145#section-4 */
    sdp += "a=setup:actpass" ENDL;
    return sdp;
}

std::string Session::constructFingerprint() const
{
    ssl::CertificateView certificateView(m_pem.certificate().x509());
    auto fingerprint = certificateView.sha256();
    std::string result = "a=fingerprint:sha-256 ";
    constexpr char hexDigits[] = "0123456789ABCDEF";
    for (size_t i = 0; i < fingerprint.size(); ++i)
    {
        if (i != 0)
            result += ':';
        result += hexDigits[(fingerprint[i] >> 4) & 0xf];
        result += hexDigits[fingerprint[i] & 0xf];
    }
    return result;
}

std::string Session::constructSdp()
{
    const auto tracks = m_tracks->allTracks();

    std::string sdp;
    std::string localAddress =
        (m_localAddress.address.isPureIpV6() ? "IP6 " : "IP4 ")
        + m_localAddress.address.toString();
#define ENDL "\r\n";
    sdp += "v=0" ENDL;
    sdp += "o=- 4833640938783375127 " + std::to_string(m_version++) + " IN " + localAddress + ENDL;
    sdp += "s=-" ENDL;
    sdp += "c=IN " + localAddress + ENDL;
    sdp += "t=0 0" ENDL;
    // Lite agent implementation.
    sdp += "a=ice-lite" ENDL;
    // Fingerprint of certificate which will be used for DTLS.
    sdp += constructFingerprint() + ENDL;
    // Group MediaID==0 (video), mediaID==1 (audio if exists) and MediaID==2 (data).
    sdp += "a=group:BUNDLE";
    for (const auto& track: tracks)
        sdp += " " + std::to_string(track.mid);
    sdp += " " + std::to_string(kDataChannelMid) + ENDL;
    // Inform that ice candidates will be sent later.
    sdp += "a=ice-options:trickle" ENDL;
    // Probably unused.
    sdp += "a=msid-semantic: WMS *" ENDL;

    sdp += "a=ice-ufrag:" + m_localUfrag + ENDL;
    sdp += "a=ice-pwd:" + m_localPwd + ENDL;

    auto port = m_localAddress.port;

    for (size_t i = 0; i < tracks.size(); ++i)
    {
        sdp += m_tracks->getSdpForTrack(&tracks[i], i == 0 ? port : 9);
        if (i == 0)
            sdp += dataChannelSdp();
        else
            sdp += std::string("a=bundle-only") + ENDL;
    }
    return sdp;
}

AnswerResult Session::processSdpAnswer(const std::string& sdp)
{
    m_sdpParseResult = SdpParseResult::parse(sdp);
    NX_DEBUG(this, "%1: Sdp parsed, got remote: %2/%3", id(), m_sdpParseResult.iceUfrag, m_sdpParseResult.icePwd);

    if (m_sdpParseResult.fingerprint.type == Fingerprint::Type::none)
    {
        NX_DEBUG(this, "%1: No DTLS fingerprint in answer", id());
        return AnswerResult::failed;
    }
    m_dtls->setFingerprint(m_sdpParseResult.fingerprint);

    if (m_sdpParseResult.tracksState[kDataChannelMid] != TrackState::active)
    {
        NX_DEBUG(
            this,
            "%1: Tracks in offer and answer does not match (application track doesn't exists)",
            id());
        return AnswerResult::failed;
    }

    bool hasInactiveTracks = false;
    for (auto& [_, track]: m_tracks->tracks())
    {
        const auto state = m_sdpParseResult.tracksState[track->mid];
        if (state == TrackState::inactive)
        {
            hasInactiveTracks = true;
            if (track->state != TrackState::offer)
                return AnswerResult::failed; //< Still failed after traying fallback codec
            if (auto it = m_providers.find(nx::Uuid(track->deviceId)); it != m_providers.end())
                it->second->setInitialized(false); //< Allow to reinit provider.
        }
        track->state = state;
    }

    if (!initializeMuxersInternal())
        return AnswerResult::failed;

    return hasInactiveTracks ? AnswerResult::again : AnswerResult::success;
}

AnswerResult Session::setFallbackCodecs()
{
    for (auto& [_, track]: m_tracks->tracks())
    {
        if (track->state == TrackState::inactive)
            return AnswerResult::failed; //< Still failed after traying fallback codec
        if (track->state == TrackState::active)
            continue;
        track->state = TrackState::inactive;
        if (auto it = m_providers.find(nx::Uuid(track->deviceId)); it != m_providers.end())
            it->second->setInitialized(false); //< Allow to reinit provider.
    }

    bool status = initializeMuxersInternal();
    return status ? AnswerResult::again : AnswerResult::failed;
}

const std::vector<IceCandidate> Session::getIceCandidates() const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_iceCandidates;
}


bool Session::lock()
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    if (isLocked())
        return false;

    NX_DEBUG(this, "Locked session %1", id());
    m_timer.invalidate();
    return true;
}

void Session::unlock()
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    NX_DEBUG(this, "Unlocked session %1", id());
    m_timer.restart();
}

bool Session::hasExpired(std::chrono::milliseconds timeout) const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_timer.isValid() && m_timer.hasExpired(timeout);
}

void Session::addIceCandidate(const IceCandidate& iceCandidate)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    int index = IceCandidate::kIceMinIndex;
    int priority = IceCandidate::kIceMaxPriority;
    if (!m_iceCandidates.empty())
    {
        index = m_iceCandidates.back().foundation + 1;
        priority = m_iceCandidates.back().priority - 1;
    }
    m_iceCandidates.push_back(iceCandidate);
    m_iceCandidates.back().foundation = index;
    m_iceCandidates.back().priority = priority;

    if (m_tracker)
        m_tracker->onIceCandidate(m_iceCandidates.back());
}

void Session::releaseTracker()
{
    auto tracker = m_tracker;
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        m_tracker.reset();
    }
}

std::shared_ptr<AbstractCameraDataProvider> Session::provider(nx::Uuid deviceId) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto it = m_providers.find(deviceId);
    return it != m_providers.end() ? it->second : nullptr;
}

std::vector<std::shared_ptr<AbstractCameraDataProvider>> Session::allProviders() const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    std::vector<std::shared_ptr<AbstractCameraDataProvider>> result;
    for (const auto& [_, p]: m_providers)
        result.push_back(p);
    return result;
}

void Session::stopAllProviders()
{
    decltype(m_providers) providers;
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        std::swap(providers, m_providers);
        m_stopped = true;
    }
    for (const auto& [_, provider]: providers)
        provider->stop();
}

} // namespace nx::webrtc
