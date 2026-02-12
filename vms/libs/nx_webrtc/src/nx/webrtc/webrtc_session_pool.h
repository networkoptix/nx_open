// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <unordered_map>

#include <nx/network/socket.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/move_only_func.h>
#include <nx/vms/common/system_context_aware.h>

#include "webrtc_ice_manager.h"
#include "abstract_camera_data_provider.h"

namespace nx::webrtc {

class IceTcp;

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
        QnFfmpegVideoTranscoder::TranscodePolicy transcodePolicy,
        const std::vector<nx::network::SocketAddress>& defaultStunServers);
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
        createDataProviderFactory providerFactory,
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
        createDataProviderFactory providerFactory,
        Purpose purpose,
        const nx::network::SocketAddress& address,
        const std::string& localUfrag,
        const SessionConfig& config);
    /**
     * @param session id
     * @return Nullptr, if session not found.
     */
    std::weak_ptr<Session> getSession(const std::string& id);

    /**
     * @param session id
     * Drop session at all, regardless of its status.
     */
    void drop(const std::string& id);

    /**
     * @return instance of IceManager
     */
    TurnInfoFetcher& turnInfoFetcher();

    // API for IceTcp.
    void gotIceTcp(std::unique_ptr<IceTcp>&& iceTcp);

    void stop();
    QnFfmpegVideoTranscoder::TranscodePolicy transcodePolicy() const;

private:
    std::vector<nx::network::SocketAddress> getStunServers();
    std::string emitUfrag();
    bool takeUfrag(const std::string& ufrag);
    std::shared_ptr<Session> createInternal(
        createDataProviderFactory providerFactory,
        Purpose purpose,
        const nx::network::SocketAddress& address,
        const std::string& localUfrag,
        const SessionConfig& config);

    /** Implementation of TimerEventHandler::onTimer. */
    void onTimer(const quint64& timerId);

private:
    mutable nx::Mutex m_mutex;
    std::unordered_map<std::string, SessionPtr> m_sessionById;
    nx::utils::TimerManager* m_timerManager = nullptr;
    TurnInfoFetcher m_turnInfoFetcher;
    quint64 m_timerId = 0;
    std::chrono::seconds m_keepAliveTimeout;
    std::list<std::unique_ptr<IceTcp>> m_tcpIces;
    QnFfmpegVideoTranscoder::TranscodePolicy m_transcodePolicy;
    std::vector<nx::network::SocketAddress> m_defaultStunServers;
};

} // namespace nx::webrtc
