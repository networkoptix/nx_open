// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ptz_promo_overlay.h"

#include <QtQuick/QQuickItem>

namespace nx::vms::client::desktop {

struct PtzPromoOverlay::Private
{
    PtzPromoOverlay* const q;
};

PtzPromoOverlay::PtzPromoOverlay(QGraphicsItem* parent, Qt::WindowFlags flags):
    GraphicsQmlView(parent, flags),
    d(new Private{this})
{
    connect(this, &GraphicsQmlView::statusChanged, this,
        [this](QQuickWidget::Status status)
        {
            if (status != QQuickWidget::Status::Ready)
                return;

            connect(rootObject(), SIGNAL(closeRequested()),
                this, SIGNAL(closeRequested()), Qt::UniqueConnection);
        });

    setSource(QUrl("Nx/Layout/Items/PtzPromoOverlay.qml"));

    quickWindow()->setColor(Qt::transparent);
}

PtzPromoOverlay::~PtzPromoOverlay()
{
    // Required here for forward-declared scoped pointer destruction.
}

} // namespace nx::vms::client::desktop
