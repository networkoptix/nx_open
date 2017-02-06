#pragma once

#include <api/server_rest_connection_fwd.h>

#include <client/client_color_types.h>

#include <core/resource/resource_fwd.h>

#include <utils/media/voice_spectrum_analyzer.h>
#include <utils/common/connective.h>

class QnTwoWayAudioWidget;
class QnImageButtonWidget;
class GraphicsLabel;
class VariantAnimator;
class QnSingleCamLicenceStatusHelper;

typedef decltype(QnSpectrumData::data) VisualizerData;

class QnTwoWayAudioWidgetPrivate: public Connective<QObject>
{
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    QnImageButtonWidget* button;
    GraphicsLabel* hint;
    QnTwoWayAudioWidgetColors colors;

public:
    QnTwoWayAudioWidgetPrivate(QnTwoWayAudioWidget* owner);
    virtual ~QnTwoWayAudioWidgetPrivate();

    void updateCamera(const QnVirtualCameraResourcePtr& camera);

    void startStreaming();
    void stopStreaming();

    void setFixedHeight(qreal height);

    void paint(QPainter *painter, const QRectF& sourceRect, const QPalette& palette);

private:
    enum HintState
    {
        OK,         /**< Hint is hidden */
        Pressed,    /**< Just pressed, waiting to check */
        Released,   /**< Button is released too fast, hint is required. */
        Error,      /**< Some error occurred */
    };

    void setState(HintState state);
    void setHint(const QString& text);

    void ensureAnimator();

    /** Check if two-way audio translation is allowed. */
    bool isAllowed() const;
private:
    Q_DECLARE_PUBLIC(QnTwoWayAudioWidget)
    QnTwoWayAudioWidget *q_ptr;

    bool m_started;
    HintState m_state;

    rest::Handle m_requestHandle;
    QTimer* m_hintTimer;
    VariantAnimator* m_hintAnimator;
    qreal m_hintVisibility;       /**< Visibility of the hint. 0 - hidden, 1 - displayed. */
    QElapsedTimer m_stateTimer;

    VisualizerData m_visualizerData;
    qint64 m_paintTimeStamp;

    QnVirtualCameraResourcePtr m_camera;
    QScopedPointer<QnSingleCamLicenceStatusHelper> m_licenseHelper;
};
