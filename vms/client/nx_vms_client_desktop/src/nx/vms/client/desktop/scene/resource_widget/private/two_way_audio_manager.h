// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/two_way_audio/two_way_audio_controller.h>

namespace nx::vms::client::desktop {

class TwoWayAudioManager: public QObject
{
    Q_OBJECT

public:
    enum class StreamingState
    {
        active,
        disabled,
        error
    };

public:
    TwoWayAudioManager(
        const QString& sourceId,
        const QnSecurityCamResourcePtr& camera,
        QObject* parent = nullptr);

    virtual ~TwoWayAudioManager();

    void startStreaming();
    void stopStreaming();

signals:
    void streamingStateChanged(StreamingState state);

private:
    nx::vms::client::core::TwoWayAudioController m_controller;
    bool m_unmuteAudioOnStreamingStop = false;

    QnSecurityCamResourcePtr m_camera;
};

} // namespace nx::vms::client::desktop