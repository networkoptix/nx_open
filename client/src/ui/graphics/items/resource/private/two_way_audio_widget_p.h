#pragma once

#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>

class QnTwoWayAudioWidget;
class QnImageButtonWidget;
class GraphicsLabel;
class VariantAnimator;

class QnTwoWayAudioWidgetPrivate: public QObject
{
    Q_OBJECT
public:
    QnImageButtonWidget* button;
    GraphicsLabel* hint;
    QnVirtualCameraResourcePtr camera;

    /** Visibility of the hint. 0 - hidden, 1 - displayed. */
    qreal hintVisibility;

    QnTwoWayAudioWidgetPrivate(QnTwoWayAudioWidget* owner);

    void startStreaming();
    void stopStreaming();

    void setFixedHeight(qreal height);

private:
    enum HintState
    {
        OK,         /**< Hint is hidden */
        Pressed,    /**< Just pressed, waiting to check */
        Released,   /**< Button is released too fast, hint is required. */
        Error,      /**< Some error occurred */
    };

    void setState(HintState state, const QString& hint = QString());

    void ensureAnimator();
private:
    Q_DECLARE_PUBLIC(QnTwoWayAudioWidget)
    QnTwoWayAudioWidget *q_ptr;



    bool started;
    rest::Handle requestHandle;
    QTimer* hintTimer;
    VariantAnimator* hintAnimator;
    HintState m_state;
    QElapsedTimer stateTimer;
};