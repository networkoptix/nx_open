#include "notifications_collection_item.h"

#include <QtGui/QGraphicsLinearLayout>

#include <ui/graphics/items/generic/image_button_widget.h>

#include <ui/style/skin.h>

QnNotificationsCollectionItem::QnNotificationsCollectionItem(QGraphicsItem *parent) :
    base_type(parent)
{
    QGraphicsWidget* controlsWidget = new QGraphicsWidget(this);

    QnImageButtonWidget* button = new QnImageButtonWidget(controlsWidget);
    button->setIcon(qnSkin->icon("item/zoom_window.png"));
    button->setCheckable(true);
    button->setToolTip(tr("Create Zoom Window"));

    QGraphicsLinearLayout *controlsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    controlsLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    controlsLayout->addStretch();
    controlsLayout->addItem(button);
    controlsLayout->addStretch();

    controlsWidget->setLayout(controlsLayout);



    QGraphicsLinearLayout *mainLayout = new QGraphicsLinearLayout(Qt::Vertical);
    mainLayout->setContentsMargins(0.5, 0.5, 0.5, 0.5);
    mainLayout->setSpacing(0.5);
    mainLayout->addItem(controlsWidget);
    mainLayout->addStretch();

    setLayout(mainLayout);

}

bool QnNotificationsCollectionItem::addSystemHealthEvent(QnSystemHealth::MessageType message) {
    return true;
}
