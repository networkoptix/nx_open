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

#include "webrtc_turn_info_fetcher.h"
#include "abstract_camera_data_provider.h"

namespace nx::webrtc {

class IceTcp;
class Reader;

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
     * Adds a session that streams video out, under a generated id. Its ownership is shared
     * between Ice, Tracker and SessionPool instances, and it is removed once no one has used it
     * for the keep-alive timeout.
     * @return Null, if the session could not be created.
     */
    std::shared_ptr<Session> createSendSession(createDataProviderFactory providerFactory,
        const nx::network::SocketAddress& address,
        const SessionConfig& config);

    /**
     * Adds a session that receives video in, under the given id. Its ownership is shared
     * between Ice, Tracker and SessionPool instances, and it is removed once no one has used it
     * for the keep-alive timeout.
     * @param reader Sink for the received frames, owned by the Device being received from.
     * @param localUfrag Forced session id.
     * @return Null, if that id is already taken, or the session could not be created.
     */
    std::shared_ptr<Session> createRecvSession(std::shared_ptr<Reader> reader,
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
    std::shared_ptr<Session> createInternal(createDataProviderFactory providerFactory,
        Purpose purpose,
        const nx::network::SocketAddress& address,
        const std::string& localUfrag,
        const SessionConfig& config,
        std::shared_ptr<Reader> reader);

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
