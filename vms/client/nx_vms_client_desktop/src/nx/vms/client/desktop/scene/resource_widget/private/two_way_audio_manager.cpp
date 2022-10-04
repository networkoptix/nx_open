// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "two_way_audio_manager.h"

#include <client/client_settings.h>
#include <client_core/client_core_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/audio/audiodevice.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/core/two_way_audio/two_way_audio_controller.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

TwoWayAudioManager::TwoWayAudioManager(
    const QString& sourceId,
    const QnSecurityCamResourcePtr& camera,
    QObject* parent)
    :
    QObject(parent),
    m_camera(camera)
{
    m_controller = std::make_unique<core::TwoWayAudioController>(
        SystemContext::fromResource(m_camera));
    m_controller->setSourceId(sourceId);
    m_controller->setResourceId(m_camera->getId());

    connect(m_controller.get(), &core::TwoWayAudioController::startedChanged, this,
        [this]
        {
            emit streamingStateChanged(m_controller->started()
                ? StreamingState::active
                : StreamingState::disabled);
        });
}

TwoWayAudioManager::~TwoWayAudioManager()
{
    stopStreaming();
}

void TwoWayAudioManager::startStreaming()
{
    if (!m_controller->available() || !m_camera || m_controller->started())
        return;

    auto server = m_camera->getParentServer();
    if (!server || server->getStatus() != nx::vms::api::ResourceStatus::online)
        return;

    const auto requestCallback =
        [this](bool success)
        {
            if (success)
            {
                if (qnSettings->muteOnAudioTransmit()
                    && !nx::audio::AudioDevice::instance()->isMute())
                {
                    m_unmuteAudioOnStreamingStop = true;
                }
            }
            else
            {
                NX_WARNING(this, "Streaming is not ready yet");
                stopStreaming();
                streamingStateChanged(StreamingState::error);
            }
        };

    if (!m_controller->start(requestCallback))
    {
        NX_WARNING(this, "Network error");
        streamingStateChanged(StreamingState::error);
    }
}

void TwoWayAudioManager::stopStreaming()
{
    if (!m_controller->available() || !m_controller->started())
        return;

    if (m_unmuteAudioOnStreamingStop)
        m_unmuteAudioOnStreamingStop = false;

    m_controller->stop();
}

} // namespace nx::vms::client::desktop