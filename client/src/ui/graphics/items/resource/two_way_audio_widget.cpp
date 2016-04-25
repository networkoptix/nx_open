#include "two_way_audio_widget.h"

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <ui/common/palette.h>

#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/resource/private/two_way_audio_widget_p.h>

QnTwoWayAudioWidget::QnTwoWayAudioWidget(QGraphicsWidget *parent)
    : base_type(parent)
    , d_ptr(new QnTwoWayAudioWidgetPrivate(this))
{
    Q_D(const QnTwoWayAudioWidget);

    d->hint->setText(tr("Hold to Speak Something"));
    d->hint->hide();

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
    d->camera = camera;
}

void QnTwoWayAudioWidget::setFixedHeight(qreal height)
{
    Q_D(QnTwoWayAudioWidget);
    d->setFixedHeight(height);

    setContentsMargins(height / 2, 0.0, 0.0, 0.0);
    updateGeometry();
}

void QnTwoWayAudioWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    base_type::paint(painter, option, widget);

    Q_D(const QnTwoWayAudioWidget);

    QRectF rect(option->rect);
    qreal w = rect.width() * d->hintVisibility;

    rect.setLeft(rect.width() - w);

    /* Skip painting if totally covered by button */
    qreal minSize = rect.height();
    if (rect.width() < minSize)
        return;

    QPainterPath path;
    path.addRoundedRect(rect, minSize / 2, minSize / 2);
    painter->fillPath(path, palette().window());
}
