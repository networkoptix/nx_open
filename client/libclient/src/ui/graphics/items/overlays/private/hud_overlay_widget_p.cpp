#include "hud_overlay_widget_p.h"

#include <QtWidgets/QGraphicsWidget>
#include <QtWidgets/QGraphicsLinearLayout>

#include <ui/graphics/items/overlays/resource_title_item.h>
#include <ui/graphics/items/overlays/hud_overlay_widget.h>

QnHudOverlayWidgetPrivate::QnHudOverlayWidgetPrivate(QnHudOverlayWidget* main):
    q_ptr(main),
    title(new QnResourceTitleItem(main)),
    details(new QnHudDetailsItem(QString(), QnHtmlTextItemOptions(), main)),
    position(new QnHudPositionItem(QString(), QnHtmlTextItemOptions(), main)),
    left(new QGraphicsWidget(main)),
    right(new QGraphicsWidget(main))
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

    auto columnsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    columnsLayout->addItem(leftLayout);
    columnsLayout->addItem(rightLayout);

    static constexpr int kMargin = 2;
    columnsLayout->setContentsMargins(kMargin, 0, kMargin, kMargin);

    auto mainLayout = new QGraphicsLinearLayout(Qt::Vertical, main);
    mainLayout->addItem(title);
    mainLayout->addItem(columnsLayout);

    static constexpr int kBorderRadius = 2;

    QnHtmlTextItemOptions options;
    options.backgroundColor = QColor(); //< to use QPalette::Window instead
    options.borderRadius = kBorderRadius;
    options.autosize = true;

    details->setOptions(options);
    position->setOptions(options);
}
