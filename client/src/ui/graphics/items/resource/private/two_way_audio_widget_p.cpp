#include "two_way_audio_widget_p.h"

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <api/server_rest_connection.h>

#include <ui/animation/variant_animator.h>
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

namespace
{
    /** If mouse button released before this timeout, show hint. */
    const int kHoldTimeoutMs = 500;

    /** How fast hint should be displayed at maximum. */
    const int kShowHintAnimationTimeMs = 300;

    /** Animation speed. 1.0 means fully expand for 1 second. */
    const qreal kShowHintAnimationSpeed = 2.0;

    /** How long hint should be displayed. */
    const int kShowHintMs = 2500;

    const qreal kHintHidden = 0.0;
    const qreal kHintVisible = 1.0;

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
    started(false),
    requestHandle(0),
    hintTimer(new QTimer(this)),
    hintAnimator(nullptr),
    hintVisibility(kHintHidden),
    m_state(OK)
{
    hint->setAcceptedMouseButtons(0);
    hint->setPerformanceHint(GraphicsLabel::PixmapCaching);
    hint->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QFont f = hint->font();
    f.setBold(true);
    hint->setFont(f);

    connect(button, &QnImageButtonWidget::pressed,  this, &QnTwoWayAudioWidgetPrivate::startStreaming);
    connect(button, &QnImageButtonWidget::released, this, &QnTwoWayAudioWidgetPrivate::stopStreaming);

    hintTimer->setInterval(100);
    hintTimer->setSingleShot(false);
    connect(hintTimer, &QTimer::timeout, this, [this]
    {
        if (m_state == OK)
            return;

        if (m_state == Pressed && stateTimer.isValid() && stateTimer.hasExpired(kHoldTimeoutMs))
            setState(OK);
        else if (m_state == Released && stateTimer.isValid() && stateTimer.hasExpired(kShowHintMs))
            setState(OK);

    });
}

void QnTwoWayAudioWidgetPrivate::startStreaming()
{
    stateTimer.invalidate();

    if (!camera)
        return;

    if (camera->getStatus() != Qn::Online && camera->getStatus() != Qn::Recording)
        return;

    auto server = camera->getParentServer();
    if (!server || server->getStatus() != Qn::Online)
        return;

    if (started)
        return;

    setState(Pressed);

    requestHandle = server->restConnection()->twoWayAudioCommand(camera->getId(), true, [this]
        (bool success, rest::Handle handle, const QnJsonRestResult& result)
    {
        if (handle != requestHandle)
            return;

        if (!success || result.error != QnRestResult::NoError)
            //stopStreaming();
                qDebug() << result.errorString;

    }, QThread::currentThread());

    if (requestHandle <= 0)
        return;

    started = true;
    static_cast<QnTwoWayAudioWidgetButton*>(button)->startAnimation();
    stateTimer.start();
}

void QnTwoWayAudioWidgetPrivate::stopStreaming()
{
    if (!started)
        return;

    setState(Released);

    started = false;
    static_cast<QnTwoWayAudioWidgetButton*>(button)->stopAnimation();

    if (!camera)
        return;

    auto server = camera->getParentServer();
    if (!server || server->getStatus() != Qn::Online)
        return;

    //TODO: #GDM What should we do if we cannot stop streaming?
    requestHandle = server->restConnection()->twoWayAudioCommand(camera->getId(), false, rest::ServerConnection::GetCallback());
}

void QnTwoWayAudioWidgetPrivate::setFixedHeight(qreal height)
{
    button->setFixedSize(height);
    hint->setMinimumHeight(height);
    hint->setMaximumHeight(height);
}

void QnTwoWayAudioWidgetPrivate::setState(HintState state, const QString& hintText)
{
    if (m_state == state)
        return;

    Q_Q(QnTwoWayAudioWidget);
    m_state = state;

    bool open = false;
    switch (m_state)
    {
    case Released:
        open = stateTimer.isValid() && !stateTimer.hasExpired(kHoldTimeoutMs);
        break;
    case Error:
        open = true;
        break;
    default:
        break;
    }

    stateTimer.restart();

    //hint->setText(hintText);
    q->updateGeometry();

    ensureAnimator();
    disconnect(hintAnimator, nullptr, this, nullptr);
    if (open)
    {
        hintAnimator->animateTo(kHintVisible);
        connect(hintAnimator, &AbstractAnimator::finished, this, [this]()
        {
            hint->show();
        });
        hintTimer->start();
    }
    else
    {
        hint->hide();
        hintAnimator->animateTo(kHintHidden);
        hintTimer->stop();
    }
}

void QnTwoWayAudioWidgetPrivate::ensureAnimator()
{
    if (hintAnimator)
        return;

    auto hintCoordGetter = [this](const QObject* target) -> qreal
    {
        Q_UNUSED(target);
        return hintVisibility;
    };

    auto hintCoordSetter = [this](const QObject* target, const QVariant& value)
    {
        Q_UNUSED(target);
        hintVisibility = value.toDouble();
    };

    AbstractAccessor* accessor = new AccessorAdaptor<decltype(hintCoordGetter), decltype(hintCoordSetter)>(hintCoordGetter, hintCoordSetter);

    Q_Q(QnTwoWayAudioWidget);
    hintAnimator = new VariantAnimator(this);
    hintAnimator->setTimer(InstrumentManager::animationTimer(q->scene()));
    hintAnimator->setAccessor(accessor);
    hintAnimator->setTargetObject(this);
    hintAnimator->setTimeLimit(kShowHintAnimationTimeMs);
    hintAnimator->setSpeed(kShowHintAnimationSpeed);
}
