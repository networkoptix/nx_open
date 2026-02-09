// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_session.h"

#include <core/resource/camera_resource.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/utils/crypt/linux_passwd_crypt.h>
#include <nx/vms/common/system_context.h>

#include "webrtc_receiver.h"
#include "webrtc_streamer.h"
#include "webrtc_tracker.h"
#include "webrtc_session_pool.h"

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
    std::unique_ptr<AbstractCameraDataProvider> provider,
    const std::string& localUfrag,
    const nx::network::SocketAddress& address,
    const std::vector<IceCandidate>& iceCandidates,
    Purpose purpose)
    :
    m_SystemContext(context),
    m_purpose(purpose),
    m_provider(std::move(provider)),
    m_localAddress(address),
    m_pem(ssl::Context::instance()->getDefaultCertificate()),
    m_tracker(std::make_shared<Tracker>(m_SystemContext, sessionPool, this)),
    m_iceCandidates(iceCandidates),
    m_sessionPool(sessionPool)
{
    m_localUfrag = localUfrag;
    m_localPwd = generatePwd(24);
    m_dtls = std::make_shared<Dtls>(m_localUfrag, m_pem);

    const auto deviceId = m_provider->resource()->getId();

    if (m_purpose == Purpose::send)
    {
        m_tracks = std::make_unique<TracksForSend>(this);

        if (m_provider->getVideoCodecParameters())
        {
            auto video = std::make_unique<Track>(Track{
                .mid = 0,
                .deviceId = deviceId,
                .trackType = TrackType::video,
                });
            m_tracks->addTrack(std::move(video));
        }

        if (m_provider->getAudioCodecParameters())
        {
            auto audio = std::make_unique<Track>(Track{
                .mid = 1,
                .deviceId = deviceId,
                .trackType = TrackType::audio,
                });
            m_tracks->addTrack(std::move(audio));
        }

        m_transceiver = std::make_unique<Streamer>(this);
        m_consumer = std::make_unique<Consumer>(this);
        m_provider->subscribe(
            [this, deviceId]
            (const QnConstAbstractMediaDataPtr& data, const AbstractCameraDataProvider::OnDataReady& handler)
            {
                m_consumer->putData(deviceId, data, handler);
            });
    }
    else if (m_purpose == Purpose::recv)
    {
        m_tracks = std::make_unique<TracksForRecv>(this);
        auto video = std::make_unique<Track>(Track{
            .mid = 0,
            .payloadType = 96,
            .deviceId = deviceId,
            .trackType = TrackType::video,
            });
        m_tracks->addTrack(std::move(video));
        auto audio = std::make_unique<Track>(Track{
            .mid = 1,
            .payloadType = 0,
            .deviceId = deviceId,
            .trackType = TrackType::audio,
        });
        m_tracks->addTrack(std::move(audio));


        m_transceiver = std::make_unique<Receiver>(this);
        m_demuxer = std::make_unique<Demuxer>(m_tracks.get());
    }
}

Session::~Session()
{
    m_reader.reset();
    releaseTracker(); //< Should be guarded.
}

bool Session::initializeMuxers(
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
    return initializeMuxersInternal(/*useFallbackVideo*/ false, /*useFallbackAudio*/ false);
}

bool Session::initializeMuxersInternal(bool useFallbackVideo, bool useFallbackAudio)
{
    // Ignore aspect ratio if user specifies the full resolution.
    if (m_resolution && m_resolution->size.width() > 0)
        m_transcodingSettings.forcedAspectRatio = QnAspectRatio();

    m_muxer = std::make_unique<Transcoder>(this, m_method, m_mseFormat);

    auto videoCodecParamaters = m_provider->getVideoCodecParameters();
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
            if (!m_provider->addVideoTranscoder(config, m_transcodingSettings))
                return false;
        }
    }

    auto audioCodecParamaters = m_provider->getAudioCodecParameters();
    if (audioCodecParamaters)
    {
        bool audioTranscodingRequired = useFallbackAudio
            || !m_muxer->isAudioCodecSupported(audioCodecParamaters->codec_id);
        if (audioTranscodingRequired)
        {
            auto audioCodecId = m_method == nx::vms::api::WebRtcMethod::srtp
                ? kFallbackSrtpAudioCodec : kFallbackMseAudioCodec;
            if (!m_provider->addAudioTranscoder(audioCodecId))
                return false;
        }
    }

    return m_muxer->createEncoders(
        m_provider->resource()->getId(),
        m_provider->getVideoCodecParameters(),
        m_provider->getAudioCodecParameters());
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

SessionDescription Session::describe() const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return SessionDescription
    {
        .id = id(),
        .remoteUfrag = getRemoteUfrag(),
        .localPwd = getLocalPwd(),
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
    sdp += "o=- 4833640938783375127 2 IN " + localAddress + ENDL;
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
    for (const auto& track: tracks)
        sdp += m_tracks->getSdpForTrack(&track, port);

    sdp += dataChannelSdp();
    return sdp;
}

AnswerResult Session::examineSdp(const std::string& sdp)
{
    m_sdpParseResult = SdpParseResult::parse(sdp);
    NX_DEBUG(this, "%1: Sdp parsed, got remote: %2/%3", id(), getRemoteUfrag(), getRemotePwd());

    if (m_sdpParseResult.fingerprint.type == Fingerprint::Type::none)
    {
        NX_DEBUG(this, "%1: No DTLS fingerprint in answer", id());
        return AnswerResult::failed;
    }
    m_dtls->setFingerprint(m_sdpParseResult.fingerprint);

#if 0
    if ((m_sdpParseResult.video != SdpParseResult::TrackState::absent) ^ (m_tracks->hasVideo() ? 1 : 0))
    {
        NX_DEBUG(
            this,
            "%1: Tracks in offer and answer does not match (video track doesn't exists)",
            id());
        return AnswerResult::failed;
    }

    if ((m_sdpParseResult.audio != SdpParseResult::TrackState::absent) ^ (m_tracks->hasAudio() ? 1 : 0))
    {
        NX_DEBUG(
            this,
            "%1: Tracks in offer and answer does not match (audio track doesn't exists)",
            id());
        return AnswerResult::failed;
    }
#endif

    if (m_sdpParseResult.application != SdpParseResult::TrackState::active)
    {
        NX_DEBUG(
            this,
            "%1: Tracks in offer and answer does not match (application track doesn't exists)",
            id());
        return AnswerResult::failed;
    }

    if (!m_tracks->examineSdp(sdp))
    {
        NX_DEBUG(this, "%1: SDP examination failed", id());
        return AnswerResult::failed;
    }

    if (!m_sdpParseResult.hasInactive())
        return AnswerResult::success;

    if (m_tracks->fallbackCodecs())
        return AnswerResult::failed;

    m_tracks->setFallbackCodecs();

    bool status = initializeMuxersInternal(
        m_sdpParseResult.video == SdpParseResult::TrackState::inactive,
        m_sdpParseResult.audio == SdpParseResult::TrackState::inactive);
    return status ? AnswerResult::again : AnswerResult::failed;
}

AnswerResult Session::setFallbackCodecs()
{
    if (m_tracks->fallbackCodecs())
        return AnswerResult::failed;
    m_tracks->setFallbackCodecs();
#if 0
    if (m_tracks->hasVideo())
        m_sdpParseResult.video = SdpParseResult::TrackState::inactive;
    if (m_tracks->hasAudio())
        m_sdpParseResult.audio = SdpParseResult::TrackState::inactive;
#endif

    bool status = initializeMuxersInternal(/*useFallbackVideo*/ true, /*useFallbackAudio*/ true);
    return status ? AnswerResult::again : AnswerResult::failed;
}

const std::vector<IceCandidate> Session::getIceCandidates() const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_iceCandidates;
}

void Session::gotIceFromManager(const IceCandidate& iceCandidate)
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

} // namespace nx::webrtc
