#pragma once

#include <api/server_rest_connection_fwd.h>

#include <client/client_color_types.h>

#include <core/resource/resource_fwd.h>

#include <utils/media/voice_spectrum_analyzer.h>

class QnTwoWayAudioWidget;
class QnImageButtonWidget;
class GraphicsLabel;
class VariantAnimator;

typedef decltype(QnSpectrumData::data) VisualizerData;

class QnTwoWayAudioWidgetPrivate: public QObject
{
    Q_OBJECT

public:
    QnImageButtonWidget* button;
    GraphicsLabel* hint;
    QnVirtualCameraResourcePtr camera;
    QnTwoWayAudioWidgetColors colors;

public:
    QnTwoWayAudioWidgetPrivate(QnTwoWayAudioWidget* owner);

    void startStreaming();
    void stopStreaming();

    void setFixedHeight(qreal height);

    void paint(QPainter *painter, const QRectF& sourceRect, const QPalette& palette);
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF& constraint, const QSizeF& baseValue) const;

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
};