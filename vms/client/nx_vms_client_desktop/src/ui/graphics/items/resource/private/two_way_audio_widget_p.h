// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/media/voice_spectrum_analyzer.h>
#include <nx/vms/license/usage_helper.h>

#include "../two_way_audio_widget.h"
#include "../voice_spectrum_painter.h"

class QnImageButtonWidget;
class GraphicsLabel;
class VariantAnimator;
class SingleCamLicenseStatusHelper;

namespace nx::vms::client::core { class TwoWayAudioController; }

class QnTwoWayAudioWidget::Private:
    public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnImageButtonWidget* const button;
    GraphicsLabel* const hint;

public:
    Private(
        const QString& sourceId,
        const QnSecurityCamResourcePtr& camera,
        QnTwoWayAudioWidget* owner);
    virtual ~Private();

    void startStreaming();
    void stopStreaming();

    void setFixedHeight(qreal height);

    void paint(QPainter* painter, const QRectF& sourceRect);

private:
    enum class HintState
    {
        ok,         //< Hint is hidden.
        hovered,    //< Initial hint showed.
        pressed,    //< Just pressed, waiting to check.
        released,   //< Button is released too fast, hint is required.
        error,      //< Some error occurred.
    };

    void updateState();
    void setState(HintState state);
    void setHint(const QString& text);

    void ensureAnimator();

    /** Check if two-way audio translation is allowed. */
    bool isAllowed() const;

private:
    QnTwoWayAudioWidget* const q;
    const QString m_sourceId;
    QnSecurityCamResourcePtr m_camera;
    std::unique_ptr<nx::vms::client::core::TwoWayAudioController> m_controller;
    HintState m_state = HintState::ok;
    QTimer* const m_hintTimer;
    VariantAnimator* m_hintAnimator = nullptr;
    qreal m_hintVisibility = 0.0; //< Visibility of the hint. 0 - hidden, 1 - displayed.
    QElapsedTimer m_stateTimer;
    bool m_unmuteAudioOnStreamingStop = false;

    nx::vms::client::desktop::VoiceSpectrumPainter m_voiceSpectrumPainter;
};
