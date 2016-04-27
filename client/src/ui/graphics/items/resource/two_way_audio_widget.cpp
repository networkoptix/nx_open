#include "two_way_audio_widget.h"

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <ui/common/palette.h>

#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/resource/private/two_way_audio_widget_p.h>

namespace
{
    /** How opaque should enabled button be painted. */
    const qreal kEnabledOpacityCoeff = 1.0;

    /** How opaque should disabled button be painted. */
    const qreal kDisabledOpacityCoeff = 0.3;
}

QnTwoWayAudioWidget::QnTwoWayAudioWidget(QGraphicsWidget *parent)
    : base_type(parent)
    , d_ptr(new QnTwoWayAudioWidgetPrivate(this))
{
    Q_D(const QnTwoWayAudioWidget);

    auto audioLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    audioLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    audioLayout->addItem(d->hint);
    audioLayout->addItem(d->button);

    setLayout(audioLayout);
    setAcceptedMouseButtons(0);
}

QnTwoWayAudioWidget::~QnTwoWayAudioWidget() {}

void QnTwoWayAudioWidget::setCamera(const QnVirtualCameraResourcePtr &camera)
{
    Q_D(QnTwoWayAudioWidget);
    Q_ASSERT(!d->camera);
    Q_ASSERT(camera);

    if (d->camera)
        disconnect(d->camera, nullptr, this, nullptr);

    d->camera = camera;

    auto updateState = [this](const QnResourcePtr &camera)
    {
        Q_D(QnTwoWayAudioWidget);
        bool enabled = camera && (camera->getStatus() == Qn::Online || camera->getStatus() == Qn::Recording);
        setEnabled(enabled);
        d->button->setEnabled(enabled);
        setOpacity(enabled ? kEnabledOpacityCoeff : kDisabledOpacityCoeff);
    };

    if (camera)
        connect(camera, &QnResource::statusChanged, this, updateState);

    updateState(camera);
}

void QnTwoWayAudioWidget::setFixedHeight(qreal height)
{
    Q_D(QnTwoWayAudioWidget);
    d->setFixedHeight(height);
}

QnTwoWayAudioWidgetColors QnTwoWayAudioWidget::colors() const
{
    Q_D(const QnTwoWayAudioWidget);
    return d->colors;
}

void QnTwoWayAudioWidget::setColors(const QnTwoWayAudioWidgetColors& value)
{
    Q_D(QnTwoWayAudioWidget);
    d->colors = value;
    update();
}

void QnTwoWayAudioWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    base_type::paint(painter, option, widget);

    Q_D(QnTwoWayAudioWidget);
    d->paint(painter, option->rect, palette());
}

QSizeF QnTwoWayAudioWidget::sizeHint(Qt::SizeHint which, const QSizeF& constraint /*= QSizeF()*/) const
{
    QSizeF base = base_type::sizeHint(which, constraint);

    Q_D(const QnTwoWayAudioWidget);
    return d->sizeHint(which, constraint, base);
}
