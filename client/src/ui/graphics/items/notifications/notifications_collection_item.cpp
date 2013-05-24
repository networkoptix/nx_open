#include "notifications_collection_item.h"

#include <QtGui/QGraphicsLinearLayout>

#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/notifications/notification_item.h>

#include <ui/style/skin.h>

QnNotificationsCollectionItem::QnNotificationsCollectionItem(QGraphicsItem *parent, Qt::WindowFlags flags) :
    base_type(parent, flags)
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

    m_layout = new QGraphicsLinearLayout(Qt::Vertical);
    m_layout->setContentsMargins(0.5, 0.5, 0.5, 0.5);
    m_layout->setSpacing(0.5);
    m_layout->addItem(controlsWidget);
    //here will be additional widget with its own layout where items will be added
    m_layout->addStretch();

    setLayout(m_layout);
}

QnNotificationsCollectionItem::~QnNotificationsCollectionItem() {
}

bool QnNotificationsCollectionItem::addSystemHealthEvent(QnSystemHealth::MessageType message) {
    QnNotificationItem *item = new QnNotificationItem(this);
    m_layout->insertItem(m_layout->count() - 2, item);

    return true;
}
