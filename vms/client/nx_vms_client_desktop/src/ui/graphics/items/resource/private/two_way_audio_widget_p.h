// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../two_way_audio_widget.h"

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QElapsedTimer>

#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/client/core/two_way_audio/two_way_audio_controller.h>
#include <nx/vms/license/usage_helper.h>
#include <utils/media/voice_spectrum_analyzer.h>

class QnImageButtonWidget;
class GraphicsLabel;
class VariantAnimator;
class SingleCamLicenseStatusHelper;

typedef decltype(QnSpectrumData::data) VisualizerData;

class QnTwoWayAudioWidget::Private:
    public QObject,
    public nx::vms::client::core::CommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnImageButtonWidget* const button;
    GraphicsLabel* const hint;

public:
    Private(const QString& sourceId, QnTwoWayAudioWidget* owner);
    virtual ~Private();

    void updateCamera(const QnSecurityCamResourcePtr& camera);

    void startStreaming();
    void stopStreaming();

    void setFixedHeight(qreal height);

    void paint(QPainter* painter, const QRectF& sourceRect);

private:
    enum class HintState
    {
        ok,         //< Hint is hidden.
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
    nx::vms::client::core::TwoWayAudioController m_controller;
    const QString m_sourceId;
    HintState m_state = HintState::ok;
    QTimer* const m_hintTimer;
    VariantAnimator* m_hintAnimator = nullptr;
    qreal m_hintVisibility = 0.0; //< Visibility of the hint. 0 - hidden, 1 - displayed.
    QElapsedTimer m_stateTimer;
    bool m_unmuteAudioOnStreamingStop = false;

    VisualizerData m_visualizerData;
    qint64 m_paintTimeStamp = 0;

    QnSecurityCamResourcePtr m_camera;
};
