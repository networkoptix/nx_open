#include "two_way_audio_widget.h"
#include "private/two_way_audio_widget_p.h"

#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QStyleOptionGraphicsItem>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <ui/common/palette.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/generic/image_button_widget.h>

QnTwoWayAudioWidget::QnTwoWayAudioWidget(const QString& sourceId, QGraphicsWidget* parent):
    base_type(parent),
    d(new Private(sourceId, this))
{
    auto audioLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    audioLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    audioLayout->addItem(d->hint);
    audioLayout->addItem(d->button);

    setLayout(audioLayout);
    setAcceptedMouseButtons(0);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QnTwoWayAudioWidget::~QnTwoWayAudioWidget()
{
}

void QnTwoWayAudioWidget::setCamera(const QnVirtualCameraResourcePtr &camera)
{
    d->updateCamera(camera);
}

void QnTwoWayAudioWidget::setFixedHeight(qreal height)
{
    d->setFixedHeight(height);
}

QnTwoWayAudioWidgetColors QnTwoWayAudioWidget::colors() const
{
    return d->colors;
}

void QnTwoWayAudioWidget::setColors(const QnTwoWayAudioWidgetColors& value)
{
    d->colors = value;
    update();
}

void QnTwoWayAudioWidget::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    base_type::paint(painter, option, widget);
    d->paint(painter, option->rect, palette());
}
