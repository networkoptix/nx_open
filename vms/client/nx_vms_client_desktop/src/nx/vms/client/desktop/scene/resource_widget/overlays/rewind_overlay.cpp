// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rewind_overlay.h"

#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QGraphicsWidget>

#include <qt_graphics_items/graphics_widget.h>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/scene/resource_widget/overlays/rewind_widget.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/window_context_aware.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_context.h>

#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>


namespace nx::vms::client::desktop {

struct RewindOverlay::Private: public QObject
{
    RewindOverlay* const q;

    Private(RewindOverlay* owner):
        q(owner),
        content(new QnViewportBoundWidget(owner)),
        fastForward(new RewindWidget(content)),
        rewind(new RewindWidget(content, false))
    {
        content->setAcceptedMouseButtons(Qt::NoButton);
        auto contentLayout = new QGraphicsLinearLayout(Qt::Horizontal, content);
        contentLayout->addItem(rewind);
        contentLayout->addStretch();
        contentLayout->addItem(fastForward);
        contentLayout->setAlignment(fastForward, Qt::AlignRight);

        connect(owner,
            &QGraphicsWidget::geometryChanged,
            this,
            &RewindOverlay::Private::updateLayout);

        connect(
            owner, &QGraphicsWidget::scaleChanged, this, &RewindOverlay::Private::updateLayout);

        connect(content,
            &QGraphicsWidget::scaleChanged,
            this,
            &RewindOverlay::Private::updateLayout);

        updateLayout();
    }

    void updateLayout()
    {
        if (content->scale() == scale && size == q->size())
            return;

        scale = content->sceneScale();
        size = q->size();

        auto scaledSize = size / scale;
        content->setGeometry({0, 0, scaledSize.width(), scaledSize.height()});
        fastForward->updateSize(scaledSize);
        rewind->updateSize(scaledSize);
    }

    QnViewportBoundWidget* const content;
    RewindWidget* const fastForward;
    RewindWidget* const rewind;

    qreal scale = 1.0;
    QSizeF size;
};

RewindOverlay::RewindOverlay(WindowContext* windowContext, QGraphicsItem* parent):
    base_type(parent),
    WindowContextAware(windowContext),
    d(new Private(this))
{
    setAcceptedMouseButtons(Qt::NoButton);

    d->fastForward->setVisible(false);
    d->rewind->setVisible(false);
    const auto fastForward =
        [this]()
        {
            if (const auto parentWidget = dynamic_cast<QnResourceWidget*>(this->parentObject()))
            {
                if (parentWidget == navigator()->currentWidget() || navigator()->syncEnabled())
                {
                    d->updateLayout();
                    d->fastForward->blink();
                    d->rewind->setVisible(false);
                }
            }
        };

    const auto rewind =
        [this]()
        {
            if (const auto parentWidget = dynamic_cast<QnResourceWidget*>(this->parentObject()))
            {
                if (parentWidget == navigator()->currentWidget() || navigator()->syncEnabled())
                {
                    d->updateLayout();
                    d->fastForward->setVisible(false);
                    d->rewind->blink();
                }
            }
        };

    connect(action(ui::action::FastForwardAction), &QAction::triggered, this, fastForward);
    connect(action(ui::action::RewindAction), &QAction::triggered, this, rewind);
}

RewindOverlay::~RewindOverlay()
{
}

RewindWidget* RewindOverlay::fastForward() const
{
    return d->fastForward;
}

QnViewportBoundWidget* RewindOverlay::content() const
{
    return d->content;
}

} // namespace nx::vms::client::desktop
