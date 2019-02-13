#pragma once

#include "../two_way_audio_widget.h"

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QElapsedTimer>

#include <api/server_rest_connection_fwd.h>
#include <client_core/connection_context_aware.h>
#include <client/client_color_types.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>
#include <utils/license_usage_helper.h>
#include <utils/media/voice_spectrum_analyzer.h>

class QnImageButtonWidget;
class GraphicsLabel;
class VariantAnimator;
class QnSingleCamLicenseStatusHelper;

typedef decltype(QnSpectrumData::data) VisualizerData;

class QnTwoWayAudioWidget::Private:
    public Connective<QObject>,
    public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    QnImageButtonWidget* const button;
    GraphicsLabel* const hint;
    QnTwoWayAudioWidgetColors colors;

public:
    Private(const QString& sourceId, QnTwoWayAudioWidget* owner);
    virtual ~Private();

    void updateCamera(const QnVirtualCameraResourcePtr& camera);

    void startStreaming();
    void stopStreaming();

    void setFixedHeight(qreal height);

    void paint(QPainter* painter, const QRectF& sourceRect, const QPalette& palette);

private:
    enum class HintState
    {
        OK,         //< Hint is hidden.
        Pressed,    //< Just pressed, waiting to check.
        Released,   //< Button is released too fast, hint is required.
        Error,      //< Some error occurred.
    };

    void setState(HintState state);
    void setHint(const QString& text);

    void ensureAnimator();

    /** Check if two-way audio translation is allowed. */
    bool isAllowed() const;

private:
    QnTwoWayAudioWidget* const q;

    const QString m_sourceId;
    bool m_started = false;
    HintState m_state = HintState::OK;

    rest::Handle m_requestHandle = 0;
    QTimer* const m_hintTimer;
    VariantAnimator* m_hintAnimator = nullptr;
    qreal m_hintVisibility = 0.0; //< Visibility of the hint. 0 - hidden, 1 - displayed.
    QElapsedTimer m_stateTimer;

    VisualizerData m_visualizerData;
    qint64 m_paintTimeStamp = 0;

    QnVirtualCameraResourcePtr m_camera;
    QScopedPointer<QnSingleCamLicenseStatusHelper> m_licenseHelper;
};
