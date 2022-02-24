// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_tracking_button.h"

#include <QtWidgets/QGraphicsLinearLayout>

#include <nx/vms/client/desktop/style/skin.h>
#include <qt_graphics_items/graphics_label.h>
#include <ui/graphics/items/generic/image_button_widget.h>

namespace nx::vms::client::desktop {

struct ObjectTrackingButton::Private
{
    ObjectTrackingButton* q;

    GraphicsLabel* label;
    QnImageButtonWidget* button;

    Private(ObjectTrackingButton* owner):
        q(owner),
        label(new GraphicsLabel(q)),
        button(new QnImageButtonWidget(q))
    {
        label->setAcceptedMouseButtons({});
        label->setPerformanceHint(GraphicsLabel::PixmapCaching);
        label->setAlignment(Qt::AlignCenter);
        label->setText(ObjectTrackingButton::tr("Object tracking is ON"));

        QFont font = label->font();
        font.setBold(true);
        font.setPixelSize(14);
        label->setFont(font);

        static const QnIcon::Suffixes suffixes({
            {QnIcon::Normal, ""},
            {QnIcon::Active, "hovered"},
            {QnIcon::Pressed, "pressed"}
        });
        const auto buttonIcon = qnSkin->icon(
            "soft_triggers/user_selectable/object_tracking_stop.png",
            QString(),
            &suffixes);

        button->setIcon(buttonIcon);
        button->setCheckable(false);
        button->setMinimumSize(16, 16);
        button->setMaximumSize(16, 16);
    }
};

ObjectTrackingButton::ObjectTrackingButton(QGraphicsItem* parent):
    base_type(parent),
    d(new Private(this))
{
    setAutoFillBackground(true);

    auto layout = new QGraphicsLinearLayout(Qt::Horizontal);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->addItem(d->label);
    layout->addItem(d->button);

    setLayout(layout);
    setAcceptedMouseButtons({});
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    connect(d->button, &QnImageButtonWidget::clicked, this, &ObjectTrackingButton::clicked);
}

ObjectTrackingButton::~ObjectTrackingButton()
{
}

} // namespace nx::vms::client::desktop