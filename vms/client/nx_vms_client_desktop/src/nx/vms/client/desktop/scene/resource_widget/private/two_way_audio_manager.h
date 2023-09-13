// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

namespace nx::vms::client::core { class TwoWayAudioController; }

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
    std::unique_ptr<core::TwoWayAudioController> m_controller;
    bool m_unmuteAudioOnStreamingStop = false;

    QnSecurityCamResourcePtr m_camera;
};

} // namespace nx::vms::client::desktop
