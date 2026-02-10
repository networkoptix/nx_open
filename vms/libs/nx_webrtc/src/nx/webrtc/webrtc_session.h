// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/ssl/certificate.h>
#include <nx/network/ssl/context.h>

#include "webrtc_reader.h"
#include "webrtc_demuxer.h"
#include "webrtc_ice.h"
#include "webrtc_consumer.h"
#include "webrtc_tracks.h"
#include "webrtc_transcoder.h"
#include "abstract_camera_data_provider.h"

namespace nx::webrtc {

static const int kDataChannelMid = 128;

enum AnswerResult
{
    success = 0,
    again,
    noop,
    failed
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
        std::unique_ptr<AbstractCameraDataProvider> provider,
        const std::string& localUfrag,
        const nx::network::SocketAddress& address,
        const QList<nx::network::SocketAddress>& allAddresses,
        const SessionConfig& config,
        const std::vector<nx::network::SocketAddress>& m_stunServers,
        Purpose purpose);
    ~Session();
    void setAddress(const nx::network::SocketAddress& address);
    std::string constructSdp();
    AnswerResult examineSdp(const std::string& sdp);
    AnswerResult setFallbackCodecs();
    Purpose purpose() const { return m_purpose; }
    std::string id() const { return m_localUfrag; }
    std::optional<SessionDescription> describe() const;
    const std::vector<IceCandidate> getIceCandidates() const;

    const nx::network::ssl::Pem& getPem() const { return m_pem; }
    void addIceCandidate(const IceCandidate& iceCandidate);
    std::shared_ptr<Tracker> tracker() { return m_tracker; }
    void releaseTracker();

    // Should be called in `unlocked` state.
    void setReader(std::shared_ptr<Reader> reader);

    SessionPool* sessionPool() const { return m_sessionPool; }
    bool initializeMuxers(
        nx::vms::api::WebRtcMethod method,
        const nx::core::transcoding::Settings& transcodingParams,
        std::optional<nx::vms::api::ResolutionData> resolution,
        std::optional<nx::vms::api::ResolutionData> resolutionWhenTranscoding,
        nx::vms::api::WebRtcTrackerSettings::MseFormat mseFormat);
    std::shared_ptr<nx::metric::Storage> metrics() const;
    Reader* reader() { return m_reader.get(); }
    Demuxer* demuxer() { return m_demuxer.get(); }
    Tracks* tracks() { return m_tracks.get(); }
    Transcoder* muxer() { return m_muxer.get(); }
    AbstractCameraDataProvider* provider() { return m_provider.get(); }
    Consumer* consumer() { return m_consumer.get(); }
    AbstractIceDelegate* transceiver() { return m_transceiver.get(); }
    void gotIceFromTracker(const IceCandidate& iceCandidate);

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

private:
    std::string dataChannelSdp() const;
    std::string constructFingerprint() const;
    bool initializeMuxersInternal(bool useFallbackVideo, bool useFallbackAudio);
    void createIces(const QList<nx::network::SocketAddress>& addresses);
    SessionDescription describeUnsafe() const;

private:
    nx::vms::common::SystemContext* m_SystemContext = nullptr;
    Purpose m_purpose = Purpose::send;
    std::unique_ptr<Tracks> m_tracks;
    std::unique_ptr<AbstractIceDelegate> m_transceiver;
    std::unique_ptr<AbstractCameraDataProvider> m_provider; //< Streaming side. Can be null.
    std::unique_ptr<Transcoder> m_muxer; //< Streaming side. Can be null.
    std::unique_ptr<Consumer> m_consumer; //< Streaming side. Can be null.
    std::unique_ptr<Demuxer> m_demuxer; //< Receiving side. Can be null.
    std::shared_ptr<Reader> m_reader; //< Receiving side. Can be null.
    std::string m_localUfrag;
    std::string m_localPwd;
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
    mutable nx::Mutex m_mutex;
    std::shared_ptr<Tracker> m_tracker;
    std::shared_ptr<Dtls> m_dtls;
    std::vector<IceCandidate> m_iceCandidates;

    // Transcoding output stream.
    nx::vms::api::WebRtcMethod m_method = nx::vms::api::WebRtcMethod::srtp;
    nx::core::transcoding::Settings m_transcodingSettings;
    std::optional<nx::vms::api::ResolutionData> m_resolution;
    std::optional<nx::vms::api::ResolutionData> m_resolutionWhenTranscoding;
    nx::vms::api::WebRtcTrackerSettings::MseFormat m_mseFormat;

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
