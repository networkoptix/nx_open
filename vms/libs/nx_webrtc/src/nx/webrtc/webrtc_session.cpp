// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_session.h"

#include <core/resource/camera_resource.h>
#include <nx/network/socket_global.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/utils/crypt/linux_passwd_crypt.h>
#include <nx/vms/common/system_context.h>

#include "webrtc_ice_relay.h"
#include "webrtc_ice_tcp.h"
#include "webrtc_ice_udp.h"
#include "webrtc_receiver.h"
#include "webrtc_session_pool.h"
#include "webrtc_srflx.h"
#include "webrtc_streamer.h"
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

// What a Device is transcoded to when the request names no codec of its own: whatever the
// delivery method carries by default.
static const AVCodecID kDefaultSrtpVideoCodec = AV_CODEC_ID_VP8;
static const AVCodecID kDefaultSrtpAudioCodec = AV_CODEC_ID_PCM_MULAW;

static const AVCodecID kDefaultMseVideoCodec = AV_CODEC_ID_H264;
static const AVCodecID kDefaultMseAudioCodec = AV_CODEC_ID_AAC;

// Neither H.264 nor Opus has a statically assigned RTP payload type, so both take one from the
// dynamic range (96-127). These are the values browsers use, which keeps the offer familiar.
constexpr int kH264PayloadType = 96;
constexpr int kOpusPayloadType = 111;

AVCodecParameters* avCodecParameters(const CodecParametersConstPtr& parameters)
{
    return parameters ? parameters->getAvCodecParameters() : nullptr;
}

}

namespace nx::webrtc {

using namespace nx::network;

std::string Session::generateLocalUfrag()
{
    return generatePwd(4);
}

Session::Session(nx::vms::common::SystemContext* context,
    SessionPool* sessionPool,
    createDataProviderFactory providerFactory,
    const std::string& localUfrag,
    const nx::network::SocketAddress& address,
    const SessionConfig& config,
    const std::vector<nx::network::SocketAddress>& stunServers,
    Purpose purpose):
    m_SystemContext(context),
    m_aioThread(nx::network::SocketGlobals::aioService().findLeastUsedAioThread()),
    m_localUfrag(localUfrag),
    m_purpose(purpose),
    m_localAddress(address),
    m_pem(ssl::Context::instance()->getDefaultCertificate()),
    m_tracker(std::make_shared<Tracker>(this)),
    m_stunServers(stunServers),
    m_sessionPool(sessionPool),
    m_config(config)
{
    m_localPwd = generatePwd(24);
    m_dtls = std::make_shared<Dtls>(m_localUfrag, m_pem);
    m_transceiver = std::make_unique<Streamer>(this);

    if (m_purpose == Purpose::send)
    {
        m_tracks = std::make_unique<TracksForSend>(this);
        m_consumer = std::make_shared<Consumer>(this);
        m_providerFactory = std::move(providerFactory);
    }
    else if (m_purpose == Purpose::recv)
    {
        m_tracks = std::make_unique<TracksForRecv>(this);
        auto video = std::make_unique<Track>(Track{
            .payloadType = kH264PayloadType,
            .trackType = TrackType::video,
            .offerState = TrackState::active,
        });
        m_tracks->addTrack(std::move(video));
        auto audio = std::make_unique<Track>(Track{
            .payloadType = kOpusPayloadType,
            .trackType = TrackType::audio,
            .offerState = TrackState::active,
        });
        m_tracks->addTrack(std::move(audio));

        m_transceiver = std::make_unique<Receiver>(this);
        m_demuxer = std::make_unique<Demuxer>(m_tracks.get());
    }
}

Session::~Session()
{
    m_sessionPool->turnInfoFetcher().removeOauthInfoHandler(id());
    m_transceiver->stop();
    m_reader.reset();
    releaseTracker(); //< Should be guarded.
}

void Session::start(const QList<nx::network::SocketAddress>& addresses)
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
        auto udpIce = std::make_unique<IceUdp>(m_sessionPool, m_config, m_aioThread);
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
    m_srflx = std::make_unique<Srflx>(m_localUfrag,
        m_stunServers,
        m_aioThread,
        [this](const nx::network::SocketAddress& mappedAddress,
            std::unique_ptr<nx::network::AbstractDatagramSocket>&& socket)
        {
            NX_DEBUG(this, "Got Srflx remote addres: %1, local address: %2",
                mappedAddress.toString(),
                socket ? socket->getLocalAddress().toString() : "null");
            // 1. Create IceUdp with given socket.
            nx::network::SocketAddress iceLocalAddress;
            if (socket && !mappedAddress.isNull())
            {
                iceLocalAddress = socket->getLocalAddress();

                NX_MUTEX_LOCKER lk(&m_mutex);
                m_punchedUdpIce = std::make_unique<IceUdp>(m_sessionPool, m_config, m_aioThread);
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

    m_relayIce = std::make_unique<IceRelay>(m_sessionPool,
        m_config,
        m_aioThread,
        id(),
        [this](const IceCandidate& candidate) { addIceCandidate(candidate); });

    m_sessionPool->turnInfoFetcher().fetchTurnInfo(
        id(),
        [this](const TurnServerInfo& info) {
            m_relayIce->startRelay(info);
        });
}

void Session::gotIceFromTracker(const IceCandidate& iceCandidate)
{
    NX_DEBUG(this, "Got ice srflx from peer: %1", iceCandidate.serialize());

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
    auto provider = m_providerFactory(deviceId, positionMs, stream, speedOpt, m_localUfrag);
    if (!provider)
    {
        NX_WARNING(this, "No data provider for device %1", deviceId);
        return nullptr;
    }
    provider->setStreamDataFilter(m_streamDataFilter);

    // The provider is keyed by the id of the resource the factory resolved, which is what the
    // calls below have to follow -- `deviceId` is only what was asked for.
    const auto providerId = provider->resource()->getId();
    if (this->provider(providerId))
    {
        // Adding it again would give it a second set of m-lines while m_providers kept the
        // provider it already has, and dropProvider() would then take down the one that streams.
        NX_WARNING(this, "Device %1 is in the session already", providerId);
        return nullptr;
    }

    addProvider(provider);
    if (!initializeMuxer(provider.get()))
    {
        NX_WARNING(this, "Can't create data provider for device %1", deviceId);
        // Leave the session as it was: the already streaming Devices keep their offer, and this
        // one is not advertised at all, so no offer round is spent on a Device that failed.
        dropProvider(providerId);
        return AbstractCameraDataProviderPtr();
    }

    sendOffer();

    return provider;
}

bool Session::removeProvider(nx::Uuid deviceId)
{
    if (!dropProvider(deviceId))
        return false;

    sendOffer();

    return true;
}

void Session::addDevice(nx::Uuid deviceId,
    std::optional<std::chrono::milliseconds> positionMs,
    nx::vms::api::StreamIndex stream,
    Consumer::StatusHandler handler)
{
    if (!NX_ASSERT(m_consumer))
        return;

    // Providers and the muxer belong to the session thread, which is not the caller's: the
    // signaling connection the call arrives on lives in a thread of its own.
    m_consumer->runInMuxingThread(
        [this, deviceId, positionMs, stream, handler = std::move(handler)]()
        {
            auto status = Consumer::StreamStatus::nodata;
            try
            {
                if (createProvider(deviceId, positionMs, stream, std::nullopt /*speedOpt*/))
                {
                    m_consumer->startProvidersIfNeeded(); // TODO It looks suspicious.
                    status = Consumer::StreamStatus::success;
                }
            }
            catch (const std::exception& e)
            {
                NX_WARNING(this, "Can't create provider: %1", e.what());
            }

            if (handler)
                handler(deviceId, status);
        });
}

void Session::removeDevice(nx::Uuid deviceId, Consumer::StatusHandler handler)
{
    if (!NX_ASSERT(m_consumer))
        return;

    m_consumer->runInMuxingThread(
        [this, deviceId, handler = std::move(handler)]()
        {
            const auto status = removeProvider(deviceId) ? Consumer::StreamStatus::success
                                                         : Consumer::StreamStatus::nodata;

            if (handler)
                handler(deviceId, status);
        });
}

RenegotiationResult Session::renegotiateProvider(
    nx::Uuid deviceId, QnAbstractMediaData::DataType dataType, int64_t resumePositionUs)
{
    auto provider = this->provider(deviceId);
    if (!provider)
    {
        NX_DEBUG(this, "Can't renegotiate unknown provider %1", deviceId);
        return RenegotiationResult::failed;
    }

    // MSE reports EOF permanently and can only be re-created with the session, see
    // Transcoder::checkForMseEof().
    if (!NX_ASSERT(m_method == nx::vms::api::WebRtcMethod::srtp,
            "Renegotiation is not applicable to MSE"))
    {
        return RenegotiationResult::failed;
    }

    Track* track = nullptr;
    if (dataType == QnAbstractMediaData::VIDEO)
        track = m_tracks->videoTrack(deviceId);
    else if (dataType == QnAbstractMediaData::AUDIO)
        track = m_tracks->audioTrack(deviceId);

    if (!track)
    {
        NX_DEBUG(this, "No %1 track of device %2 to renegotiate", dataType, deviceId);
        return RenegotiationResult::failed;
    }

    // Nothing is packetized for an m-line that is not offered as active, so a renegotiation of
    // one can only come from a caller that made it up. createRtpEncoders() would put the m-line
    // back in the offer as a live one, which is the one thing a refused m-line must never be.
    if (track->offerState != TrackState::active)
    {
        NX_DEBUG(this,
            "Track %1 of device %2 is not offered, nothing to renegotiate",
            track->mid,
            deviceId);
        return RenegotiationResult::failed;
    }

    // Without this, every packet that hits the still-EOF encoder would start another round.
    if (track->pendingAnswer)
        return RenegotiationResult::pending;

    // Nothing sent before the client applies the new SDP would be decodable anyway.
    provider->suspend();
    if (m_consumer)
        m_consumer->clearQueueForDevice(deviceId);

    // The recreated RTP encoder restarts its sequence numbers and timestamps, so on the old SSRC
    // it would look to the receiver like a stale continuation of the same stream rather than a
    // new one.
    const auto oldSsrc = track->ssrc;
    const auto* oldEncoder = encoderForTrack(track);
    if (!m_tracks->updateSsrc(track))
    {
        NX_DEBUG(this, "Can't assign a new ssrc to track %1 of device %2", track->mid, deviceId);
        return RenegotiationResult::failed;
    }

    track->pendingAnswer = true;
    track->resumePositionUs = resumePositionUs;

    // createRtpEncoders() reports success if either track got an encoder, so check this one:
    // otherwise the offer would advertise an SSRC that the (still EOF) old encoder never emits,
    // and the next packet would trigger another renegotiation.
    if (!initializeMuxer(provider.get(), dataType) || encoderForTrack(track) == oldEncoder)
    {
        NX_DEBUG(
            this, "No encoder for the new codec of track %1, device %2", track->mid, deviceId);
        track->pendingAnswer = false;
        NX_ASSERT(m_tracks->updateSsrc(track, oldSsrc), "Can't roll the ssrc back");
        return RenegotiationResult::failed;
    }

    sendOffer();

    return RenegotiationResult::offerSent;
}

const QnUniversalRtpEncoder* Session::encoderForTrack(const Track* track) const
{
    if (!m_muxer)
        return nullptr;

    if (track->trackType == TrackType::audio)
        return m_muxer->audioEncoder(track->deviceId);

    return m_muxer->videoEncoder(track->deviceId);
}

bool Session::isRenegotiationPending(nx::Uuid deviceId) const
{
    for (const auto* track: {m_tracks->videoTrack(deviceId), m_tracks->audioTrack(deviceId)})
    {
        if (track && track->pendingAnswer)
            return true;
    }
    return false;
}

std::shared_ptr<AbstractCameraDataProvider> Session::takeProvider(nx::Uuid deviceId)
{
    std::shared_ptr<AbstractCameraDataProvider> provider;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        auto it = m_providers.find(deviceId);
        if (it == m_providers.end())
            return nullptr;
        provider = it->second;
        m_providers.erase(it);
    }
    provider->stop();
    return provider;
}

bool Session::dropProvider(nx::Uuid deviceId)
{
    if (!takeProvider(deviceId))
        return false;

    // Nothing of this Device is to be sent any more: what it has left in the queue would be
    // packetized under the ssrc of an m-line that is not its own by then, and its encoders would
    // outlive it for the rest of the session.
    if (m_consumer)
        m_consumer->clearQueueForDevice(deviceId);
    if (m_muxer)
        m_muxer->removeEncoders(deviceId);

    // The m-lines of the Device stay in the offer, rejected, until another Device takes them:
    // the client has seen them and they cannot disappear. Dropping the Device off them keeps
    // them from describing a Device that streams nothing, and from shadowing the real tracks if
    // the client adds the same Device again.
    // NOTE: Tracks belongs to the session AIO thread, which every caller of this runs in. The
    // lock only mirrors addProvider(), where these tracks were added.
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_tracks->freeTracks(deviceId);

    return true;
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
            .deviceId = deviceId,
            .trackType = TrackType::video,
            });
        m_tracks->addTrack(std::move(video));
    }

    if (provider->getAudioCodecParameters())
    {
        auto audio = std::make_unique<Track>(Track{
            .deviceId = deviceId,
            .trackType = TrackType::audio,
            });
        m_tracks->addTrack(std::move(audio));
    }

    m_providers.emplace(deviceId, provider);
    provider->setDataConsumer(m_consumer);
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

bool Session::initializeMuxer(
    AbstractCameraDataProvider* provider, QnAbstractMediaData::DataType videoOrAudio)
{
    // Ignore aspect ratio if user specifies the full resolution.
    if (m_resolution && m_resolution->size.width() > 0)
        m_transcodingSettings.forcedAspectRatio = QnAspectRatio();

    // A single track is (re)initialized on renegotiation, so that the other one keeps the encoder
    // it already streams with, RTP sequence numbers included. See renegotiateProvider().
    const bool doVideo = videoOrAudio != QnAbstractMediaData::AUDIO;
    const bool doAudio = videoOrAudio != QnAbstractMediaData::VIDEO;

    auto videoCodecParamaters = doVideo ? provider->getVideoCodecParameters() : nullptr;
    if (videoCodecParamaters)
    {
        bool videoTranscodingRequired = !m_transcodingSettings.isEmpty() || m_resolution
            || !m_muxer->isVideoCodecSupported(videoCodecParamaters->getCodecId());

        const auto transcodePolicy = m_sessionPool->transcodePolicy();
        if (transcodePolicy.disableTranscoding && videoTranscodingRequired)
        {
            NX_DEBUG(this,
                "Video transcoding is disabled(source codec: %1) in the server settings. "
                "Feature unavailable",
                videoCodecParamaters->getCodecId());
            return false;
        }

        if (videoTranscodingRequired)
        {
            QnFfmpegVideoTranscoder::Config config(transcodePolicy);
            config.targetCodecId = m_method == nx::vms::api::WebRtcMethod::srtp
                ? kDefaultSrtpVideoCodec
                : kDefaultMseVideoCodec;

            // A request that names the codec gets that codec rather than the default of the
            // delivery method: the client knows what it can decode and says so.
            if (const auto& requested = m_transcodingSettings.videoCodec)
            {
                const auto requestedCodecId = nx::transcoding::findEncoderCodecId(*requested);
                if (requestedCodecId == AV_CODEC_ID_NONE)
                {
                    NX_DEBUG(this, "No encoder for the requested video codec '%1'", *requested);
                    return false;
                }

                if (!m_muxer->isVideoCodecSupported(requestedCodecId))
                {
                    NX_DEBUG(this,
                        "The requested video codec %1 can't be delivered via %2",
                        requestedCodecId,
                        m_method);
                    return false;
                }

                config.targetCodecId = requestedCodecId;
            }

            config.quality = Qn::StreamQuality::normal;
            if (m_method == nx::vms::api::WebRtcMethod::srtp)
                config.rtpContatiner = true;

            if (m_resolution)
                config.outputResolutionLimit = m_resolution->size;
            else if (m_resolutionWhenTranscoding)
                config.outputResolutionLimit = m_resolutionWhenTranscoding->size;
            config.useMultiThreadEncode = nx::transcoding::useMultiThreadEncode(
                config.targetCodecId, config.outputResolutionLimit);

            config.params = suggestMediaStreamParams(config.targetCodecId, config.quality);
            if (m_method == nx::vms::api::WebRtcMethod::mse)
            {
                // Gop size should be less then MAX_FRAME_DURATION (5s) for correct delay
                // calculation in Consumer::calculateDelay(). You can only specify gop_size in
                // number of frames, so 20/ is enough for fps >= 5.
                config.params.insert(QnCodecParams::gop_size, 20);
            }
            if (!provider->addVideoTranscoder(config, m_transcodingSettings))
                return false;

            // The encoder is created for the transcoder output, not for the source.
            videoCodecParamaters = provider->getVideoCodecParameters();

            NX_DEBUG(this, "Video transcoding started, target codec: %1", config.targetCodecId);
        }
    }

    auto audioCodecParamaters = doAudio ? provider->getAudioCodecParameters() : nullptr;
    if (audioCodecParamaters)
    {
        bool audioTranscodingRequired =
            !m_muxer->isAudioCodecSupported(audioCodecParamaters->getCodecId());
        if (audioTranscodingRequired)
        {
            auto audioCodecId = m_method == nx::vms::api::WebRtcMethod::srtp
                ? kDefaultSrtpAudioCodec
                : kDefaultMseAudioCodec;
            if (!provider->addAudioTranscoder(audioCodecId))
                return false;

            audioCodecParamaters = provider->getAudioCodecParameters();

            NX_DEBUG(this, "Audio transcoding started, target codec: %1", audioCodecId);
        }
    }

    if (!m_muxer->createEncoders(provider->resource()->getId(),
            avCodecParameters(videoCodecParamaters),
            avCodecParameters(audioCodecParamaters)))
    {
        return false;
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
    const auto port = m_localAddress.port;

    // An m-line with nothing to describe is still offered, rejected, so that the client keeps
    // every m-line it has seen. Such an m-line is out of the BUNDLE group, which may name only
    // m-lines the offer carries, or the client rejects the offer whole.
    std::string mediaSections;
    std::string bundleMids;
    for (const auto& [_, track]: m_tracks->tracks())
    {
        const auto media = m_tracks->getSdpForTrack(track.get(), port);
        if (media.empty())
        {
            mediaSections += m_tracks->rejectedSdpForTrack(track.get());
            continue;
        }

        bundleMids += " " + std::to_string(track->mid);
        mediaSections += media;
        mediaSections += "a=bundle-only\r\n";
    }
    bundleMids += " " + std::to_string(kDataChannelMid);

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
    // Every m-line the offer carries, the data channel one last.
    sdp += "a=group:BUNDLE" + bundleMids + ENDL;
    // Inform that ice candidates will be sent later.
    sdp += "a=ice-options:trickle" ENDL;
    // Probably unused.
    sdp += "a=msid-semantic: WMS *" ENDL;

    sdp += "a=ice-ufrag:" + m_localUfrag + ENDL;
    sdp += "a=ice-pwd:" + m_localPwd + ENDL;

    sdp += mediaSections;
    sdp += dataChannelSdp();
    return sdp;
}

bool Session::processSdpAnswer(const std::string& sdp)
{
    m_sdpParseResult = SdpParseResult::parse(sdp);
    NX_DEBUG(this, "Sdp parsed, got remote: %1/%2", m_sdpParseResult.iceUfrag, m_sdpParseResult.icePwd);

    if (m_sdpParseResult.fingerprint.type == Fingerprint::Type::none)
    {
        NX_DEBUG(this, "No DTLS fingerprint in answer");
        return false;
    }
    m_dtls->setFingerprint(m_sdpParseResult.fingerprint);

    if (m_sdpParseResult.tracksState[kDataChannelMid] != TrackState::active)
    {
        NX_DEBUG(
            this, "Tracks in offer and answer does not match (application track doesn't exists)");
        return false;
    }

    if (!m_tracks->examineSdp(sdp))
    {
        NX_DEBUG(this, "SDP examination failed");
        return false;
    }

    // There is no second offer to try, so the answer is the last word on every track. One the
    // client did not take is not offered as active any more, and nothing is packetized for it,
    // see Transcoder::setDataPacket().
    //
    // How many of its offered tracks the client took, per Device: one that is in here with none
    // taken streams nothing at all.
    std::map<nx::Uuid, int> acceptedPerDevice;
    std::vector<Track*> refused;
    int offeredTracks = 0;
    int acceptedTracks = 0;

    const auto& answered = m_sdpParseResult.tracksState;
    for (auto& [_, track]: m_tracks->tracks())
    {
        if (track->offerState != TrackState::active)
            continue;

        // An answer carries an m-line for each one of the offer it answers (RFC 3264 section 6),
        // so a mid it says nothing about belongs to an m-line it has never been offered -- a
        // Device added while the client was still answering an earlier offer. Reading that
        // silence as a refusal would drop a Device the client has not been asked about yet, or
        // end the session, if it were the only one streaming by then.
        const auto state = answered.find(track->mid);
        if (state == answered.end())
            continue;

        ++offeredTracks;
        auto& accepted = acceptedPerDevice[track->deviceId];
        if (state->second == TrackState::active)
        {
            ++accepted;
            ++acceptedTracks;
            continue;
        }

        NX_DEBUG(
            this, "Track %1 of device %2 was rejected by the client", track->mid, track->deviceId);
        track->offerState = TrackState::inactive;
        refused.push_back(track.get());
    }

    // Nothing of what was offered is going to be played, and no later offer can change that, so
    // let the client see the session end instead of waiting out the keep-alive for media that
    // will never come.
    if (offeredTracks > 0 && acceptedTracks == 0)
    {
        NX_DEBUG(this, "The client took none of the %1 tracks offered", offeredTracks);
        return false;
    }

    // A Device left without a single track streams nothing, and the client has said as much
    // itself, so there is nothing to renegotiate and no offer to send.
    for (const auto& [deviceId, accepted]: acceptedPerDevice)
    {
        if (accepted > 0 || deviceId.isNull())
            continue;

        NX_DEBUG(this, "No track of device %1 was taken by the client, drop it", deviceId);
        dropProvider(deviceId);
    }

    // A refused m-line is answered with port 0 and left out of the BUNDLE group, which stops the
    // transceiver behind it for good, so offering it again as a live, bundled m-line under the
    // same mid is what RFC 8829 section 5.2.2 forbids -- and the client is free to reject the
    // whole offer over it, taking down the tracks it did take along with it. Hand the m-line
    // back instead: from here on it is offered rejected, like any other free one, until
    // addTrack() gives it to another Device under a mid of its own.
    for (Track* track: refused)
    {
        if (track->deviceId.isNull())
            continue; //< Its Device took nothing of the offer and is gone, m-lines and all.

        // Whatever is encoded for it is discarded on arrival, and the encoder would otherwise
        // outlive the m-line, for getEncryptor() and getLastTimestamps() to pick.
        if (m_muxer)
        {
            m_muxer->removeEncoder(track->deviceId,
                track->trackType == TrackType::audio ? QnAbstractMediaData::AUDIO
                                                     : QnAbstractMediaData::VIDEO);
        }

        // NOTE: the lock mirrors the one in dropProvider(), see the note there.
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_tracks->freeTrack(track);
    }

    // This answer is final, there is no other round: any track renegotiated mid-session (see
    // renegotiateProvider()) now has a client that knows its new SSRC/payload type, so its
    // provider (suspended and purged of stale data since the offer was sent) can restart.
    std::map<nx::Uuid, int64_t> resumePositionsUs;
    for (auto& [_, track]: m_tracks->tracks())
    {
        if (!track->pendingAnswer)
            continue;
        track->pendingAnswer = false;
        resumePositionsUs[track->deviceId] = track->resumePositionUs;
    }

    // Restart each renegotiated provider now that the client knows about its new SSRC/payload
    // type, the same way an explicit seek would -- for a clean, keyframe-aligned resume.
    if (m_consumer)
    {
        for (const auto& [deviceId, resumePositionUs]: resumePositionsUs)
            m_consumer->seek(deviceId, resumePositionUs, [](nx::Uuid, Consumer::StreamStatus) {});
    }

    return true;
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

    NX_DEBUG(this, "Locked session");
    m_timer.invalidate();
    return true;
}

void Session::unlock()
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    NX_DEBUG(this, "Unlocked session");
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

std::shared_ptr<Tracker> Session::tracker()
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_tracker;
}

void Session::sendOffer()
{
    if (auto tracker = this->tracker(); tracker && tracker->isStarted())
        tracker->sendOffer();
}

void Session::releaseTracker()
{
    std::shared_ptr<Tracker> tracker; //< Destroyed after the lock is released.
    NX_MUTEX_LOCKER lk(&m_mutex);
    std::swap(tracker, m_tracker);
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

std::string Session::idForToStringFromPtr() const
{
    return m_localUfrag;
}

} // namespace nx::webrtc
