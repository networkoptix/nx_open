#include "notifications_collection_item.h"

#include <QtGui/QGraphicsLinearLayout>

#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/notifications/notification_item.h>
#include <ui/graphics/items/notifications/notification_list_widget.h>

#include <ui/style/skin.h>

QnNotificationsCollectionItem::QnNotificationsCollectionItem(QGraphicsItem *parent, Qt::WindowFlags flags) :
    base_type(parent, flags)
{
    QGraphicsWidget* controlsWidget = new QGraphicsWidget(this);

    QnImageButtonWidget* button = new QnImageButtonWidget(controlsWidget);
    button->setIcon(qnSkin->icon("item/zoom_window.png"));
    button->setCheckable(true);
    button->setToolTip(tr("Create Zoom Window"));
    button->setGeometry(QRectF(0, 0, 36, 36));

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

    m_list = new QnNotificationListWidget(this);
    m_layout->addItem(m_list);
    m_layout->addStretch();

    setLayout(m_layout);
}

QnNotificationsCollectionItem::~QnNotificationsCollectionItem() {
}

bool QnNotificationsCollectionItem::addSystemHealthEvent(QnSystemHealth::MessageType message) {
    QnNotificationItem *item = new QnNotificationItem(m_list);
    m_list->addItem(item);

    return true;
}
