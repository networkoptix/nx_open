#include "two_way_audio_widget.h"

#include <QtWidgets/QGraphicsRectItem>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <api/server_rest_connection.h>

#include <ui/style/skin.h>
#include <ui/workaround/sharp_pixmap_painting.h>

#include <utils/common/connective.h>
#include <utils/common/delayed.h>

class QnTwoWayAudioWidgetPrivate
{
public:
    QnVirtualCameraResourcePtr camera;
    QPixmap micPixmap;
    QScopedPointer<QMovie> micAnimation;
    bool started;
    rest::Handle requestHandle;

    QnTwoWayAudioWidgetPrivate();

    void startStreaming();
    void stopStreaming();
};

QnTwoWayAudioWidgetPrivate::QnTwoWayAudioWidgetPrivate() :
    camera(),
    micPixmap(qnSkin->pixmap("item/mic.png")),
    micAnimation(qnSkin->newMovie("item/mic_bg_animation.gif")),
    started(false),
    requestHandle(0)
{
    if (micAnimation->loopCount() >= 0)
        QObject::connect(micAnimation.data(), &QMovie::finished, micAnimation.data(), &QMovie::start);
}

void QnTwoWayAudioWidgetPrivate::startStreaming()
{
    if (!camera)
        return;

    if (camera->getStatus() != Qn::Online && camera->getStatus() != Qn::Recording)
        return;

    auto server = camera->getParentServer();
    if (!server || server->getStatus() != Qn::Online)
        return;

    if (started)
        return;

    requestHandle = server->restConnection()->twoWayAudioCommand(camera->getId(), true, [this]
        (bool success, rest::Handle handle, const QnJsonRestResult& result)
    {
        if (handle != requestHandle)
            return;

        if (!success || result.error != QnRestResult::NoError)
            stopStreaming();

    }, QThread::currentThread());

    if (requestHandle <= 0)
        return;

    started = true;
    micAnimation->start();
}

void QnTwoWayAudioWidgetPrivate::stopStreaming()
{
    if (!started)
        return;

    started = false;
    micAnimation->stop();

    if (!camera)
        return;

    auto server = camera->getParentServer();
    if (!server || server->getStatus() != Qn::Online)
        return;

    //TODO: #GDM What should we do if we cannot stop streaming?
    requestHandle = server->restConnection()->twoWayAudioCommand(camera->getId(), false, rest::ServerConnection::GetCallback());
}

/******* QnTwoWayAudioWidget *******/

QnTwoWayAudioWidget::QnTwoWayAudioWidget(QGraphicsWidget *parent)
    : base_type(lit("two_way_autio"), parent)
    , d_ptr(new QnTwoWayAudioWidgetPrivate())
{
    setCheckable(false);

    setPixmap(StateFlag::Default, qnSkin->pixmap("item/mic_bg.png"));
    setPixmap(StateFlag::Hovered, qnSkin->pixmap("item/mic_bg_hovered.png"));
    setPixmap(StateFlag::Pressed, qnSkin->pixmap("item/mic_bg_pressed.png"));
}

QnTwoWayAudioWidget::~QnTwoWayAudioWidget() {}

void QnTwoWayAudioWidget::setCamera(const QnVirtualCameraResourcePtr &camera)
{
    Q_D(QnTwoWayAudioWidget);
    Q_ASSERT(!d->camera);
    Q_ASSERT(camera);
    d->camera = camera;
}

void QnTwoWayAudioWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    base_type::paint(painter, option, widget);

    Q_D(QnTwoWayAudioWidget);
    if (isPressed())
        paintPixmapSharp(painter, d->micAnimation->currentPixmap());

    paintPixmapSharp(painter, d->micPixmap);
}

void QnTwoWayAudioWidget::pressedNotify(QGraphicsSceneMouseEvent *event)
{
    base_type::pressedNotify(event);

    Q_D(QnTwoWayAudioWidget);
    d->startStreaming();
}

void QnTwoWayAudioWidget::releasedNotify(QGraphicsSceneMouseEvent *event)
{
    base_type::releasedNotify(event);

    Q_D(QnTwoWayAudioWidget);
    d->stopStreaming();
}
