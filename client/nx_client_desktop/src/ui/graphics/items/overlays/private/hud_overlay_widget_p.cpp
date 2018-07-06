#include "hud_overlay_widget_p.h"

#include <QtWidgets/QGraphicsWidget>
#include <QtWidgets/QGraphicsLinearLayout>

#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/overlays/resource_title_item.h>
#include <ui/graphics/items/overlays/hud_overlay_widget.h>
#include <ui/style/helper.h>

QnHudOverlayWidgetPrivate::QnHudOverlayWidgetPrivate(QnHudOverlayWidget* main):
    base_type(),
    q_ptr(main),
    titleHolder(new QnViewportBoundWidget(main)),
    title(new QnResourceTitleItem(titleHolder)),
    content(new QnViewportBoundWidget(main)),
    details(new QnHudDetailsItem()),
    position(new QnHudPositionItem()),
    left(new QGraphicsWidget(content)),
    right(new QGraphicsWidget(content))
{
    auto leftLayout = new QGraphicsLinearLayout(Qt::Vertical);
    left->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
    left->setAcceptedMouseButtons(Qt::NoButton);
    leftLayout->addItem(left);
    leftLayout->addItem(details);
    leftLayout->setAlignment(details, Qt::AlignLeft | Qt::AlignBottom);

    auto rightLayout = new QGraphicsLinearLayout(Qt::Vertical);
    right->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
    right->setAcceptedMouseButtons(Qt::NoButton);
    rightLayout->addItem(right);
    rightLayout->addItem(position);
    rightLayout->setAlignment(position, Qt::AlignRight | Qt::AlignBottom);

    auto contentLayout = new QGraphicsLinearLayout(Qt::Horizontal, content);
    contentLayout->addItem(leftLayout);
    contentLayout->addItem(rightLayout);

    content->setAcceptedMouseButtons(Qt::NoButton);
    titleHolder->setAcceptedMouseButtons(Qt::NoButton);

    auto titleLayout = new QGraphicsLinearLayout(Qt::Vertical, titleHolder);
    titleLayout->addItem(title);
    titleLayout->addStretch();

    connect(main, &QGraphicsWidget::geometryChanged,
        this, &QnHudOverlayWidgetPrivate::updateLayout);
    connect(title, &QGraphicsWidget::geometryChanged,
        this, &QnHudOverlayWidgetPrivate::updateLayout);
    connect(titleHolder, &QnViewportBoundWidget::scaleChanged,
        this, &QnHudOverlayWidgetPrivate::updateLayout);

    static constexpr int kBorderRadius = 2;

    QnHtmlTextItemOptions options;
    options.backgroundColor = QColor(); //< to use QPalette::Window instead
    options.borderRadius = kBorderRadius;
    options.autosize = true;

    details->setOptions(options);
    position->setOptions(options);
}

void QnHudOverlayWidgetPrivate::updateLayout()
{
    Q_Q(const QnHudOverlayWidget);
    titleHolder->setFixedSize(q->size());

    static const auto kSpacing = style::Metrics::kDefaultLayoutSpacing.height();
    const auto titleSpace = (title->size().height() + kSpacing) * titleHolder->scale();
    content->setFixedSize(q->size() - QSizeF(0.0, titleSpace));
    content->setPos(0.0, titleSpace);
}
