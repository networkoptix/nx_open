// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "hud_overlay_widget_p.h"

#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QGraphicsWidget>

#include <nx/vms/client/desktop/scene/resource_widget/overlays/resource_bottom_item.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/overlays/hud_overlay_widget.h>
#include <ui/graphics/items/overlays/resource_title_item.h>

QnHudOverlayWidgetPrivate::QnHudOverlayWidgetPrivate(QnHudOverlayWidget* main):
    base_type(),
    q_ptr(main),
    titleHolder(new QnViewportBoundWidget(main)),
    title(new QnResourceTitleItem(titleHolder)),
    bottomHolder(new QnViewportBoundWidget(main)),
    bottom(new nx::vms::client::desktop::ResourceBottomItem(bottomHolder)),
    content(new QnViewportBoundWidget(main)),
    details(new QnHtmlTextItem()),
    left(new QGraphicsWidget(content)),
    right(new QGraphicsWidget(content)),
    actionIndicator(new QnActionIndicatorItem(content))
{
    auto leftLayout = new QGraphicsLinearLayout(Qt::Vertical);
    left->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
    left->setAcceptedMouseButtons(Qt::NoButton);
    leftLayout->addItem(left);
    leftLayout->addItem(actionIndicator);
    leftLayout->addItem(details);
    leftLayout->setAlignment(details, Qt::AlignLeft | Qt::AlignBottom);

    auto rightLayout = new QGraphicsLinearLayout(Qt::Vertical);
    right->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
    right->setAcceptedMouseButtons(Qt::NoButton);
    rightLayout->addItem(right);
    rightLayout->addItem(bottomHolder);
    rightLayout->setAlignment(bottomHolder, Qt::AlignRight | Qt::AlignBottom);

    auto contentLayout = new QGraphicsLinearLayout(Qt::Horizontal, content);
    contentLayout->addItem(leftLayout);
    contentLayout->addItem(rightLayout);

    content->setAcceptedMouseButtons(Qt::NoButton);
    titleHolder->setAcceptedMouseButtons(Qt::NoButton);
    bottomHolder->setAcceptedMouseButtons(Qt::NoButton);

    auto titleLayout = new QGraphicsLinearLayout(Qt::Vertical, titleHolder);
    titleLayout->addItem(title);
    titleLayout->addStretch();

    auto bottomLayout = new QGraphicsLinearLayout(Qt::Vertical, bottomHolder);
    bottomLayout->addItem(bottom);
    bottomLayout->setAlignment(bottom, Qt::AlignRight | Qt::AlignBottom);

    connect(main, &QGraphicsWidget::geometryChanged,
        this, &QnHudOverlayWidgetPrivate::updateLayout);

    connect(title, &QGraphicsWidget::geometryChanged,
        this, &QnHudOverlayWidgetPrivate::updateLayout);

    connect(titleHolder, &QnViewportBoundWidget::scaleChanged,
        this, &QnHudOverlayWidgetPrivate::updateLayout);

    connect(details, &QnHtmlTextItem::opacityChanged, this,
        [this]() { details->setVisible(!qFuzzyIsNull(details->opacity())); });

    static constexpr int kBorderRadius = 2;

    QnHtmlTextItemOptions options;
    options.backgroundColor = QColor(); //< to use QPalette::Window instead
    options.borderRadius = kBorderRadius;
    options.autosize = true;

    details->setOptions(options);
}

void QnHudOverlayWidgetPrivate::updateLayout()
{
    Q_Q(const QnHudOverlayWidget);
    titleHolder->setFixedSize(q->size());

    static const auto kSpacing = nx::style::Metrics::kDefaultLayoutSpacing.height();
    const auto titleSpace = (title->size().height() + kSpacing) * titleHolder->scale();
    content->setFixedSize(q->size() - QSizeF(0.0, titleSpace));
    content->setPos(0.0, titleSpace);
}
