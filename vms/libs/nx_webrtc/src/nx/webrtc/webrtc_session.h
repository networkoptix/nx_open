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
        const std::vector<IceCandidate>& iceCandidates,
        Purpose purpose);
    ~Session();
    void setAddress(const nx::network::SocketAddress& address);
    std::string constructSdp();
    AnswerResult examineSdp(const std::string& sdp);

    AnswerResult setFallbackCodecs();

    Purpose purpose() const { return m_purpose; }
    std::string id() const { return getLocalUfrag(); }
    std::string getLocalUfrag() const { return m_localUfrag; }
    std::string getLocalPwd() const { return m_localPwd; }
    std::string getRemoteUfrag() const { return m_sdpParseResult.iceUfrag; }
    std::string getRemotePwd() const { return m_sdpParseResult.icePwd; }
    const SdpParseResult& sdpParseResult() const { return m_sdpParseResult; }
    SessionDescription describe() const;
    const std::vector<IceCandidate> getIceCandidates() const;

    const nx::network::ssl::Pem& getPem() const { return m_pem; }
    void gotIceFromManager(const IceCandidate& iceCandidate);
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

private:
    void addTrackAttributes(const Track& track, const std::string& localAddress, std::string& sdp);
    void addDataChannelAttributes(const std::string& localAddress, std::string& sdp);
    std::string constructFingerprint() const;
    bool initializeMuxersInternal(bool useFallbackVideo, bool useFallbackAudio);

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
    std::string m_localMsid;
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
    SessionPool* m_sessionPool;
};

} // namespace nx::webrtc
