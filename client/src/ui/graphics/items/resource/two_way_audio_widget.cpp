#include "two_way_audio_widget.h"

#include <QtWidgets/QGraphicsRectItem>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <api/app_server_connection.h>
#include <api/server_rest_connection.h>

#include <utils/common/connective.h>
#include <utils/common/delayed.h>

#include <ui/style/skin.h>
#include <ui/workaround/sharp_pixmap_painting.h>

class QnTwoWayAudioWidgetPrivate
{
public:
    QnVirtualCameraResourcePtr camera;
    QPixmap micPixmap;
    QScopedPointer<QMovie> micAnimation;

    QnTwoWayAudioWidgetPrivate();
};

QnTwoWayAudioWidgetPrivate::QnTwoWayAudioWidgetPrivate() :
    micPixmap(qnSkin->pixmap("item/mic.png")),
    micAnimation(qnSkin->newMovie("item/mic_bg_animation.gif"))
{
    if (micAnimation->loopCount() >= 0)
        QObject::connect(micAnimation.data(), &QMovie::finished, micAnimation.data(), &QMovie::start);
}

QnTwoWayAudioWidget::QnTwoWayAudioWidget(QGraphicsWidget *parent)
    : base_type(lit("two_way_autio"), parent)
    , d_ptr(new QnTwoWayAudioWidgetPrivate())
{
    setCheckable(false);

    setPixmap(StateFlag::Default, qnSkin->pixmap("item/mic_bg.png"));
    setPixmap(StateFlag::Hovered, qnSkin->pixmap("item/mic_bg_hovered.png"));
    setPixmap(StateFlag::Pressed, qnSkin->pixmap("item/mic_bg_pressed.png"));

    connect(this, &QnImageButtonWidget::pressed, d_ptr->micAnimation.data(), &QMovie::start);
    connect(this, &QnImageButtonWidget::released, d_ptr->micAnimation.data(), &QMovie::stop);
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
    {
        paintPixmapSharp(painter, d->micAnimation->currentPixmap());
        //painter->drawPixmap(option->rect, d->micAnimation->currentPixmap());
    }

    paintPixmapSharp(painter, d->micPixmap);
    //painter->drawPixmap(option->rect, d->micPixmap);
}
