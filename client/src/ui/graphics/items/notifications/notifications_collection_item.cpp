#include "notifications_collection_item.h"

#include <QtGui/QGraphicsLinearLayout>

#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/notifications/notification_item.h>
#include <ui/graphics/items/notifications/notification_list_widget.h>

#include <ui/style/skin.h>

QnNotificationsCollectionItem::QnNotificationsCollectionItem(QGraphicsItem *parent, Qt::WindowFlags flags) :
    base_type(parent, flags)
{
    m_headerWidget = new QGraphicsWidget(this);

    QnImageButtonWidget* button = new QnImageButtonWidget(m_headerWidget);
    button->setIcon(qnSkin->icon("item/zoom_window.png"));
    button->setCheckable(true);
    button->setToolTip(tr("Settings, bla-bla"));
    button->setMinimumSize(QSizeF(24, 24));
    button->setMaximumSize(QSizeF(24, 24));

    QGraphicsLinearLayout *controlsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    controlsLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    controlsLayout->addStretch();
    controlsLayout->addItem(button);
    m_headerWidget->setLayout(controlsLayout);

    QGraphicsLinearLayout* layout = new QGraphicsLinearLayout(Qt::Vertical);
    layout->setContentsMargins(0.5, 0.5, 0.5, 0.5);
    layout->setSpacing(0.5);
    layout->addItem(m_headerWidget);

    m_list = new QnNotificationListWidget(this);
    layout->addItem(m_list);
    layout->setStretchFactor(m_list, 1.0);

    setLayout(layout);
}

QnNotificationsCollectionItem::~QnNotificationsCollectionItem() {
}

bool QnNotificationsCollectionItem::addSystemHealthEvent(QnSystemHealth::MessageType message) {
    QnNotificationItem *item = new QnNotificationItem(m_list);
    m_list->addItem(item);

    return true;
}

QRectF QnNotificationsCollectionItem::headerGeometry() const {
    return m_headerWidget->geometry();
}
