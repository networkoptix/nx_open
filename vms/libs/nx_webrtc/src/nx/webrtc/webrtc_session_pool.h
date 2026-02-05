// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QThreadPool>
#include <chrono>
#include <memory>
#include <optional>
#include <unordered_map>

#include <nx/network/socket.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/timer_manager.h>
#include <nx/vms/common/system_context_aware.h>

#include "webrtc_ice_manager.h"
#include "abstract_camera_data_provider.h"

namespace nx::webrtc {

/**
 * - Owns Session objects.
 * - Removes session if no one uses it during specified timeout. Session cannot be removed while its id is locked.
 * NOTE: It is recommended to lock session id before performing any operations with session.
 * NOTE: Specifying session timeout and working with no id lock can result in undefined behavior.
 * NOTE: Class methods are thread-safe.
 */
class NX_WEBRTC_API SessionPool:
    public QObject,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT

public:
    SessionPool(
        nx::vms::common::SystemContext* context,
        nx::utils::TimerManager* timerManager,
        QnFfmpegVideoTranscoder::TranscodePolicy transcodePolicy);
    virtual ~SessionPool();

    /**
     * Add new session.
     * @param session Object ownership is shared between Ice, Tracker and SessionPool instances.
     * @param purpose Session type: for streaming video or for receiving video.
     * @param keepAliveTimeoutSec Session will be removed,
     * if no one uses it for keepAliveTimeoutSec seconds. 0 is treated as no timeout.
     * @return False, if session with id session->id() already exists. true, otherwise.
     */
    std::shared_ptr<Session> create(
        std::unique_ptr<AbstractCameraDataProvider> cameraProvider,
        Purpose purpose,
        const nx::network::SocketAddress& address,
        const SessionConfig& config);
    /**
     * Add new session.
     * @param session Object ownership is shared between Ice, Tracker and SessionPool instances.
     * @param purpose Session type: for streaming video or for receiving video.
     * @param localUfrag Forced session id.
     * @param keepAliveTimeoutSec Session will be removed,
     * if no one uses it for keepAliveTimeoutSec seconds. 0 is treated as no timeout.
     * @return False, if session with id session->id() already exists. true, otherwise.
     */
    std::shared_ptr<Session> create(
        std::unique_ptr<AbstractCameraDataProvider> cameraProvider,
        Purpose purpose,
        const nx::network::SocketAddress& address,
        const std::string& localUfrag,
        const SessionConfig& config);
    /**
     * @param session id
     * @return Nullptr, if session not found.
     * NOTE: Cancel session deletion timer for keepAliveTimeoutSec.
     */
    std::shared_ptr<Session> lock(const std::string& id);
    /**
     * @param session id
     * @return Nullopt, if session not found.
     */
    std::optional<SessionDescription> describe(const std::string& id);
    /**
     * @param session id
     * NOTE: Starts session deletion timer for keepAliveTimeoutSec if no refs found.
     */
    void unlock(const std::string& id);
    /**
     * @param session id
     * Drop session at all, regardless of its status.
     */

    void drop(const std::string& id);
    /**
     * @return instance of IceManager
     */
    IceManager& iceManager();
    /*
     * @param sessionId
     * @param iceCandidate - candidate description as it received from Ice
     * A method for Ices to notify Tracker about new candidate.
     * */
    void gotIceFromManager(const std::string& sessionId, const IceCandidate& iceCandidate);
    /*
     * @return Dedicated thread pool for Trackers.
     * */
    QThreadPool* threadPool();

    void stop();

    QnFfmpegVideoTranscoder::TranscodePolicy transcodePolicy() const;

private:
    struct SessionContext
    {
        SessionPtr session;
        SessionConfig config;
        nx::utils::ElapsedTimer timer{ nx::utils::ElapsedTimerState::started};

        void lock() { timer.invalidate(); }
        bool isLocked() const { return !timer.isValid(); }
        void unlock() { timer.restart(); }
        bool hasExpired(std::chrono::milliseconds timeout) const
        {
            return timer.isValid() && timer.hasExpired(timeout);
        }
    };

private:
    std::string emitUfrag();
    bool takeUfrag(const std::string& ufrag);
    std::shared_ptr<Session> createInternal(
        std::unique_ptr<AbstractCameraDataProvider> cameraProvider,
        Purpose purpose,
        const nx::network::SocketAddress& address,
        const std::string& localUfrag,
        const SessionConfig& config);
    std::shared_ptr<Session> createSessionInternal(
        std::unique_ptr<AbstractCameraDataProvider> cameraProvider,
        Purpose purpose,
        const nx::network::SocketAddress& address,
        const std::string& localUfrag,
        const std::vector<IceCandidate>& iceCandidates,
        const SessionConfig& config);

    /** Implementation of TimerEventHandler::onTimer. */
    void onTimer(const quint64& timerId);

private:
    mutable nx::Mutex m_mutex;
    std::unordered_map<std::string, std::optional<SessionContext>> m_sessionById;
    nx::utils::TimerManager* m_timerManager = nullptr;
    IceManager m_iceManager;
    std::unique_ptr<QThreadPool> m_threadPool;
    quint64 m_timerId = 0;
    std::chrono::seconds m_keepAliveTimeout;
    QnFfmpegVideoTranscoder::TranscodePolicy m_transcodePolicy;
};

} // namespace nx::webrtc
