#include "two_way_audio_widget_p.h"

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <api/server_rest_connection.h>

#include <ui/animation/variant_animator.h>
#include <ui/animation/opacity_animator.h>
#include <ui/common/accessor.h>
#include <ui/common/palette.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/resource/two_way_audio_widget.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/style/skin.h>
#include <ui/workaround/sharp_pixmap_painting.h>

#include <utils/common/connective.h>
#include <utils/common/delayed.h>
#include <utils/common/scoped_painter_rollback.h>

namespace
{
    /** If mouse button released before this timeout, show hint. */
    const int kHoldTimeoutMs = 500;

    /** How fast hint should be displayed at maximum. */
    const int kShowHintAnimationTimeMs = 500;

    /** Animation speed. 1.0 means fully expand for 1 second. */
    const qreal kShowHintAnimationSpeed = 2.0;

    /** Animation speed. 1.0 means show/hide for 1 second. */
    const qreal kHintOpacityAnimationSpeed = 3.0;

    /** How long hint should be displayed. */
    const int kShowHintMs = 2000;

    /** How long error should be displayed. */
    const int kShowErrorMs = 2000;

    const qreal kHidden = 0.0;
    const qreal kVisible = 1.0;

    class QnTwoWayAudioWidgetButton: public QnImageButtonWidget
    {
    public:
        QnTwoWayAudioWidgetButton(QGraphicsWidget *parent = nullptr) :
            QnImageButtonWidget(lit("two_way_autio"), parent),
            m_micPixmap(qnSkin->pixmap("item/mic.png")),
            m_micAnimation(qnSkin->newMovie("item/mic_bg_animation.gif"))
        {
            setCheckable(false);

            setPixmap(QnImageButtonWidget::StateFlag::Default, qnSkin->pixmap("item/mic_bg.png"));
            setPixmap(QnImageButtonWidget::StateFlag::Hovered, qnSkin->pixmap("item/mic_bg_hovered.png"));
            setPixmap(QnImageButtonWidget::StateFlag::Pressed, qnSkin->pixmap("item/mic_bg_pressed.png"));

            if (m_micAnimation->loopCount() >= 0)
                QObject::connect(m_micAnimation.data(), &QMovie::finished, m_micAnimation.data(), &QMovie::start);
        }

        void startAnimation()
        {
            m_micAnimation->start();
        }

        void stopAnimation()
        {
            m_micAnimation->stop();
        }

    protected:
        virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
        {
            QnImageButtonWidget::paint(painter, option, widget);
            if (isPressed())
                paintPixmapSharp(painter, m_micAnimation->currentPixmap());

            paintPixmapSharp(painter, m_micPixmap);
        }

    private:
        QPixmap m_micPixmap;
        QScopedPointer<QMovie> m_micAnimation;
    };

}


QnTwoWayAudioWidgetPrivate::QnTwoWayAudioWidgetPrivate(QnTwoWayAudioWidget* owner) :
    q_ptr(owner),
    button(new QnTwoWayAudioWidgetButton(owner)),
    hint(new GraphicsLabel(owner)),
    camera(),
    m_started(false),
    m_state(OK),
    m_requestHandle(0),
    m_hintTimer(new QTimer(this)),
    m_hintAnimator(nullptr),
    m_hintVisibility(kHidden),
    m_stateTimer()
{
    hint->setAcceptedMouseButtons(0);
    hint->setPerformanceHint(GraphicsLabel::PixmapCaching);
    hint->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QFont f = hint->font();
    f.setBold(true);
    hint->setFont(f);
    hint->setOpacity(kHidden);

    connect(button, &QnImageButtonWidget::pressed,  this, &QnTwoWayAudioWidgetPrivate::startStreaming);
    connect(button, &QnImageButtonWidget::released, this, &QnTwoWayAudioWidgetPrivate::stopStreaming);

    m_hintTimer->setInterval(100);
    m_hintTimer->setSingleShot(false);
    connect(m_hintTimer, &QTimer::timeout, this, [this]
    {
        if (!m_stateTimer.isValid())
            return;

        int timeout = 0;
        switch (m_state)
        {
        case OK:
            return;
        case Pressed:
            timeout = kHoldTimeoutMs;
            break;
        case Released:
            timeout = kShowHintMs;
            break;
        case Error:
            timeout = kShowErrorMs;
            break;
        default:
            Q_ASSERT_X(false, Q_FUNC_INFO, "Invalid case");
            return;
        }

        if (m_stateTimer.hasExpired(timeout))
            setState(OK);
    });
}

void QnTwoWayAudioWidgetPrivate::startStreaming()
{
    m_stateTimer.invalidate();

    Q_Q(QnTwoWayAudioWidget);
    if (!q->isEnabled())
        return;

    if (!camera)
        return;

    auto server = camera->getParentServer();
    if (!server || server->getStatus() != Qn::Online)
        return;

    if (m_started)
        return;

    setState(Pressed);

    m_requestHandle = server->restConnection()->twoWayAudioCommand(camera->getId(), true, [this]
        (bool success, rest::Handle handle, const QnJsonRestResult& result)
    {
        if (handle != m_requestHandle)
            return;

        if (!success || result.error != QnRestResult::NoError)
        {
            setState(Error, result.errorString);
            stopStreaming();
        }

    }, QThread::currentThread());

    if (m_requestHandle <= 0)
        return;

    m_started = true;
    static_cast<QnTwoWayAudioWidgetButton*>(button)->startAnimation();
    m_stateTimer.start();
}

void QnTwoWayAudioWidgetPrivate::stopStreaming()
{
    if (!m_started)
        return;

    if (m_state != Error)
        setState(Released);

    m_started = false;
    static_cast<QnTwoWayAudioWidgetButton*>(button)->stopAnimation();

    if (!camera)
        return;

    auto server = camera->getParentServer();
    if (!server || server->getStatus() != Qn::Online)
        return;

    //TODO: #GDM What should we do if we cannot stop streaming?
    if (m_state != Error)
        m_requestHandle = server->restConnection()->twoWayAudioCommand(camera->getId(), false, rest::ServerConnection::GetCallback());
}

void QnTwoWayAudioWidgetPrivate::setFixedHeight(qreal height)
{
    button->setFixedSize(height);
    hint->setMinimumHeight(height);
    hint->setMaximumHeight(height);
}

void QnTwoWayAudioWidgetPrivate::paint(QPainter *painter, const QRectF& sourceRect, const QPalette& palette)
{
    QRectF rect(sourceRect);
    qreal w = rect.width() * m_hintVisibility;

    rect.setLeft(rect.width() - w);

    /* Skip painting if totally covered by button */
    qreal minSize = button->geometry().width(); //;
    if (rect.width() < minSize)
        return;

    QPainterPath path;
    path.addRoundedRect(rect, minSize / 2, minSize / 2);
    painter->fillPath(path, palette.window());
}

void QnTwoWayAudioWidgetPrivate::setState(HintState state, const QString& hintText)
{
    if (m_state == state)
        return;

    m_state = state;

    bool open = false;
    switch (m_state)
    {
    case Released:
        if (m_stateTimer.isValid() && !m_stateTimer.hasExpired(kHoldTimeoutMs))
        {
            open = true;
            Q_Q(QnTwoWayAudioWidget);
            hint->setText(tr("Hold to Speak Something"));
            q->updateGeometry();
        }
        break;
    case Error:
        {
            open = true;
            Q_Q(QnTwoWayAudioWidget);
            hint->setText(hintText);
            q->updateGeometry();
        }
        break;
    default:
        break;
    }

    m_stateTimer.restart();

    ensureAnimator();
    disconnect(m_hintAnimator, nullptr, this, nullptr);
    if (open)
    {
        connect(m_hintAnimator, &AbstractAnimator::finished, this, [this]()
        {
            opacityAnimator(hint, kHintOpacityAnimationSpeed)->animateTo(kVisible);
        });
        m_hintAnimator->setEasingCurve(QEasingCurve::OutCubic);
        m_hintAnimator->animateTo(kVisible);
        m_hintTimer->start();
    }
    else
    {
        m_hintAnimator->setEasingCurve(QEasingCurve::InCubic);
        m_hintAnimator->animateTo(kHidden);
        m_hintTimer->stop();
        opacityAnimator(hint, kHintOpacityAnimationSpeed)->animateTo(kHidden);
    }
}

void QnTwoWayAudioWidgetPrivate::ensureAnimator()
{
    if (m_hintAnimator)
        return;

    auto hintCoordGetter = [this](const QObject* target) -> qreal
    {
        Q_UNUSED(target);
        return m_hintVisibility;
    };

    auto hintCoordSetter = [this](const QObject* target, const QVariant& value)
    {
        Q_UNUSED(target);
        m_hintVisibility = value.toDouble();
        Q_Q(QnTwoWayAudioWidget);
        q->update();
    };

    AbstractAccessor* accessor = new AccessorAdaptor<decltype(hintCoordGetter), decltype(hintCoordSetter)>(hintCoordGetter, hintCoordSetter);

    Q_Q(QnTwoWayAudioWidget);
    m_hintAnimator = new VariantAnimator(this);
    m_hintAnimator->setEasingCurve(QEasingCurve::InOutCubic);
    m_hintAnimator->setTimer(InstrumentManager::animationTimer(q->scene()));
    m_hintAnimator->setAccessor(accessor);
    m_hintAnimator->setTargetObject(this);
    m_hintAnimator->setTimeLimit(kShowHintAnimationTimeMs);
    m_hintAnimator->setSpeed(kShowHintAnimationSpeed);
}
