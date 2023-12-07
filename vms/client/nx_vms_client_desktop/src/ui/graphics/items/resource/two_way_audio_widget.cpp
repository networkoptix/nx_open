// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "two_way_audio_widget.h"

#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QStyleOptionGraphicsItem>

#include <qt_graphics_items/graphics_label.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <ui/common/palette.h>
#include <ui/graphics/items/generic/image_button_widget.h>

#include "private/two_way_audio_widget_p.h"

using namespace nx::vms::client::desktop;

QnTwoWayAudioWidget::QnTwoWayAudioWidget(
    const QString& sourceId,
    const QnSecurityCamResourcePtr& camera,
    QGraphicsWidget* parent)
    :
    base_type(parent),
    d(new Private(sourceId, camera, this))
{
    auto audioLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    audioLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    audioLayout->addItem(d->hint);
    audioLayout->addItem(d->button);

    setLayout(audioLayout);
    setAcceptedMouseButtons(Qt::NoButton);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setPaletteColor(this, QPalette::Window,
        colorTheme()->color("camera.twoWayAudio.background.default"));
    setPaletteColor(this, QPalette::WindowText,
        colorTheme()->color("camera.twoWayAudio.text"));
}

QnTwoWayAudioWidget::~QnTwoWayAudioWidget()
{
}

void QnTwoWayAudioWidget::setFixedHeight(qreal height)
{
    d->setFixedHeight(height);
}

QnImageButtonWidget *QnTwoWayAudioWidget::button() const {
    return d->button;
}

void QnTwoWayAudioWidget::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    base_type::paint(painter, option, widget);
    d->paint(painter, option->rect);
}
