// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/ssl/certificate.h>
#include <nx/network/ssl/context.h>

#include "abstract_camera_data_provider.h"
#include "webrtc_consumer.h"
#include "webrtc_demuxer.h"
#include "webrtc_ice.h"
#include "webrtc_reader.h"
#include "webrtc_tracks.h"
#include "webrtc_transcoder.h"

namespace nx::webrtc {

static const int kDataChannelMid = 128;

enum class CodecSwitchResult
{
    done, //< The encoder now streams the new codec, on the same m-line and the same SSRC.
    notNegotiated, //< The client did not take this codec, nothing can be sent to it any more.
    failed, //< No encoder for the new codec; the caller should fall back to a reconnect.
};

class SessionPool;
class Tracker;
class AbstractIceDelegate;
class IceUdp;
class IceRelay;
class Srflx;
class Session;
class ConnectionProcessor;

class NX_WEBRTC_API Session
{
public:
    static std::string generateLocalUfrag();

    Session(
        nx::vms::common::SystemContext* context,
        SessionPool* sessionPool,
        createDataProviderFactory providerFactory,
        const std::string& localUfrag,
        const nx::network::SocketAddress& address,
        const SessionConfig& config,
        const std::vector<nx::network::SocketAddress>& m_stunServers,
        Purpose purpose);
    ~Session();

    void start(const QList<nx::network::SocketAddress>& addresses);
    void setAddress(const nx::network::SocketAddress& address);
    // The tag of the BUNDLE group: the m-line carrying the transport every bundled one shares,
    // and the one a trickled ICE candidate names, see Tracker::sendIce().
    struct BundleTag
    {
        int mid = kDataChannelMid;
        int mLineIndex = 0;
    };

    // Builds the next offer and reports the tag of its BUNDLE group.
    // NOTE: the tag moves from one offer to the next, so a candidate sent with it is good only
    // for a client that has applied this offer. Both go out over the same WebSocket, in order.
    std::string constructSdp(BundleTag* bundleTag = nullptr);
    // Applies the answer to this session. Returns false if it leaves the session unusable.
    bool processSdpAnswer(const std::string& sdp);
    Purpose purpose() const { return m_purpose; }
    std::string id() const { return m_localUfrag; }
    std::optional<SessionDescription> describe() const;
    const std::vector<IceCandidate> getIceCandidates() const;

    const nx::network::ssl::Pem& getPem() const { return m_pem; }
    void addIceCandidate(const IceCandidate& iceCandidate);
    std::shared_ptr<Tracker> tracker();
    void releaseTracker();

    // Should be called in `unlocked` state.
    void setReader(std::shared_ptr<Reader> reader);

    SessionPool* sessionPool() const { return m_sessionPool; }

    /**
     * Every pollable object owned by this session (Consumer, Tracker, Srflx and all the ICEs
     * created here) lives in this single AIO thread. Keeping them together is what allows
     * Consumer to be bound to its AIO thread once and never be rebound: moving a BasicPollable
     * between AIO threads while posted calls for it are still queued in the old one is undefined
     * behavior (see BasicPollable::bindToAioThread), and the posts come from provider threads at
     * arbitrary moments, so there is no safe point to do it at.
     * NOTE: IceTcp is the exception: it is created by the TCP listener before any session is
     * known, so it keeps its own AIO thread. That is fine, since every Ice method Consumer uses
     * is thread-safe.
     */
    nx::network::aio::AbstractAioThread* aioThread() const { return m_aioThread; }
    void setStreamDataFilter(nx::vms::api::StreamDataFilters filter) { m_streamDataFilter = filter; }
    void setTranscodingParams(nx::vms::api::WebRtcMethod method,
        const nx::core::transcoding::Settings& transcodingParams,
        std::optional<nx::vms::api::ResolutionData> resolution,
        std::optional<nx::vms::api::ResolutionData> resolutionWhenTranscoding,
        nx::vms::api::WebRtcTrackerSettings::MseFormat mseFormat,
        std::optional<QString> videoCodec = std::nullopt);
    std::shared_ptr<nx::metric::Storage> metrics() const;
    Reader* reader() { return m_reader.get(); }
    Demuxer* demuxer() { return m_demuxer.get(); }
    Tracks* tracks() { return m_tracks.get(); }
    Transcoder* muxer() { return m_muxer.get(); }
    std::shared_ptr<AbstractCameraDataProvider> provider(nx::Uuid deviceId) const;
    std::vector<std::shared_ptr<AbstractCameraDataProvider>> allProviders() const;
    Consumer* consumer() { return m_consumer.get(); }
    AbstractIceDelegate* transceiver() { return m_transceiver.get(); }
	void gotIceFromTracker(const IceCandidate& iceCandidate);
    void stopAllProviders();

    AbstractCameraDataProviderPtr createProvider(
        nx::Uuid deviceId,
        std::optional<std::chrono::milliseconds> positionMs,
        nx::vms::api::StreamIndex stream,
        std::optional<float> speedOpt);

    /**
     * Adds a Device to the session and starts streaming it: the asynchronous counterpart of
     * createProvider(), for a caller that is not in the session thread. The work is posted to
     * aioThread(), the one that owns the providers and the muxer, and `handler` -- which may be
     * empty -- is called there with `success`, or with `nodata` for a Device that could not be
     * added, an unknown one and one the session already has among them.
     */
    void addDevice(nx::Uuid deviceId,
        std::optional<std::chrono::milliseconds> positionMs,
        nx::vms::api::StreamIndex stream,
        Consumer::StatusHandler handler = nullptr);

    /**
     * Removes a Device from the session: the asynchronous counterpart of removeProvider(). See
     * addDevice() for the threading and for `handler`, which gets `nodata` for a Device the
     * session does not have.
     */
    void removeDevice(nx::Uuid deviceId, Consumer::StatusHandler handler = nullptr);

    /**
     * @return false, if session already locked.
     * NOTE: Cancel session deletion timer for keepAliveTimeoutSec and lock the session for other
     * ices.
     */
    bool lock();
    /**
     * NOTE: Starts session deletion timer for keepAliveTimeoutSec if no refs found.
     */
    void unlock();
    bool hasExpired(std::chrono::milliseconds timeout) const;

    bool removeProvider(nx::Uuid deviceId);

    /**
     * Rebuilds the `videoOrAudio` encoder of a streaming Device around the codec its media now
     * carries -- a live-to-archive seek, a camera reconfiguration -- and leaves everything the
     * client knows about the track untouched: the m-line, its mid and its SSRC stay as they are,
     * and so do the sequence numbers, the timestamps and the SRTP context of the stream, which
     * goes on as one continuous RTP stream whose packets merely carry another payload type from
     * here on. No SDP is exchanged, which is what the m-line offering every codec at once buys
     * (see Transcoder::createRtpEncoder()): the client has already accepted the payload type,
     * so there is nothing left to ask it. SRTP only, MSE has no payload types.
     *
     * @return notNegotiated if the client left this codec out of its answer -- the only thing
     *     left then is to tell it so, since no offer of ours can add a codec it cannot decode;
     *     failed if the encoder could not be built at all, which leaves a reconnect.
     */
    CodecSwitchResult switchCodec(nx::Uuid deviceId, QnAbstractMediaData::DataType videoOrAudio);

    /**
     * Whether the client took the payload type every active track of `deviceId` streams under.
     * An answer accepts an m-line as a whole, formats of its own and all, so it may well take
     * the track and leave out the very codec the Device streams on it -- and a packet sent
     * under a payload type the answer does not list is dropped by the receiver unread. A track
     * no answer has covered yet counts as negotiated, as it does in switchCodec(). SRTP only:
     * MSE carries a mime type instead of payload types.
     */
    bool isStreamedCodecNegotiated(nx::Uuid deviceId);

    std::string idForToStringFromPtr() const;

private:
    std::string dataChannelSdp(bool isBundleTag) const;
    std::string constructFingerprint() const;
    // Builds the encoders of one provider, and of a single track of it unless UNKNOWN is passed.
    // `provider` has to be in the session already: the encoders are built around the tracks that
    // addProvider() made for it. Not to be called twice for the same provider unless its encoders
    // are meant to be rebuilt -- it re-opens the transcoder of the Device, which is what a codec
    // change asks for and what an ordinary caller does not.
    bool initializeMuxer(AbstractCameraDataProvider* provider,
        QnAbstractMediaData::DataType videoOrAudio = QnAbstractMediaData::UNKNOWN);
    const QnUniversalRtpEncoder* encoderForTrack(const Track* track) const;
    void addProvider(std::shared_ptr<AbstractCameraDataProvider> provider);
    // Detaches the provider of `deviceId` from the session and stops it, leaving its tracks alone.
    std::shared_ptr<AbstractCameraDataProvider> takeProvider(nx::Uuid deviceId);
    // Undoes createProvider() for a Device that never reached an offer: no SDP is sent and
    // nothing of the Device is left to describe, unlike removeProvider(), which retires the
    // m-lines the client has already seen.
    bool dropProvider(nx::Uuid deviceId);
    void sendOffer();
    SessionDescription describeUnsafe() const;

private:
    mutable nx::Mutex m_mutex;

    nx::vms::common::SystemContext* m_SystemContext = nullptr;
    nx::network::aio::AbstractAioThread* const m_aioThread;
    std::string m_localPwd;
    const std::string m_localUfrag;
    Purpose m_purpose = Purpose::send;
    std::unique_ptr<Tracks> m_tracks;
    std::unique_ptr<AbstractIceDelegate> m_transceiver;
    std::map<nx::Uuid, std::shared_ptr<AbstractCameraDataProvider>> m_providers; //< Streaming side. Can be null.
    std::unique_ptr<Transcoder> m_muxer; //< Streaming side. Can be null.
    std::shared_ptr<Consumer> m_consumer; //< Streaming side. Can be null.
    std::unique_ptr<Demuxer> m_demuxer; //< Receiving side. Can be null.
    std::shared_ptr<Reader> m_reader; //< Receiving side. Can be null.
    SdpParseResult m_sdpParseResult;
    nx::network::SocketAddress m_localAddress;
    nx::network::ssl::Pem m_pem;

    /* Note that Session pointer is shared between Tracker, SessionPool, and one Ice
     * (which locked this Session first).
     * Session life-time comprises 2 state: locked and unlocked.
     * Session's state change is provided by methods SessionPool::lock() and SessionPool::unlock().
     * In 'unlocked' state, Session should be used only from Tracker.
     * In 'locked' state, Session should be used only from Ice.
     * But, the fields 'm_tracker', 'm_iceCandidates' and 'm_dtls' can be used in any state.
     * So, access to these fields should be guarded by this mutex.
     * */
    std::shared_ptr<Tracker> m_tracker;
    std::shared_ptr<Dtls> m_dtls;
    std::vector<IceCandidate> m_iceCandidates;

    // Transcoding output stream.
    nx::vms::api::WebRtcMethod m_method = nx::vms::api::WebRtcMethod::srtp;
    nx::core::transcoding::Settings m_transcodingSettings;
    // The video codec the request asked for, if any; it wins over the default of the delivery
    // method. Unlike the settings above it names an encoder, not a filter, so it is kept apart.
    std::optional<QString> m_videoCodec;
    std::optional<nx::vms::api::ResolutionData> m_resolution;
    std::optional<nx::vms::api::ResolutionData> m_resolutionWhenTranscoding;
    nx::vms::api::WebRtcTrackerSettings::MseFormat m_mseFormat;

    bool m_stopped = false;
    nx::vms::api::StreamDataFilters m_streamDataFilter{nx::vms::api::StreamDataFilter::media};
    createDataProviderFactory m_providerFactory;
    int m_version = 1;

    std::vector<std::unique_ptr<IceUdp>> m_udpIces;
    std::unique_ptr<IceUdp> m_punchedUdpIce;
    std::unique_ptr<IceRelay> m_relayIce;
    std::unique_ptr<Srflx> m_srflx;
    std::unique_ptr<IceCandidate> m_remoteSrflx;
    std::vector<nx::network::SocketAddress> m_stunServers;
    SessionPool* m_sessionPool;
    SessionConfig m_config;


    // Timer for carbage collector in SessionPool.
    nx::utils::ElapsedTimer m_timer{ nx::utils::ElapsedTimerState::started};
    bool isLocked() const { return !m_timer.isValid(); }
};

} // namespace nx::webrtc
