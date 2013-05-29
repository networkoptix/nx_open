#include "notifications_collection_item.h"

#include <QtGui/QGraphicsLinearLayout>

#include <core/resource/resource.h>

#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/notifications/notification_item.h>
#include <ui/graphics/items/notifications/notification_list_widget.h>
#include <ui/style/skin.h>

QnNotificationsCollectionItem::QnNotificationsCollectionItem(QGraphicsItem *parent, Qt::WindowFlags flags) :
    base_type(parent, flags)
{
    m_headerWidget = new QGraphicsWidget(this);

    QnImageButtonWidget* hideAllButton = new QnImageButtonWidget(m_headerWidget);
    hideAllButton->setIcon(qnSkin->icon("tree/servers.png"));
    hideAllButton->setToolTip(tr("Hide all"));
    hideAllButton->setMinimumSize(QSizeF(24, 24));
    hideAllButton->setMaximumSize(QSizeF(24, 24));

    QnImageButtonWidget* settingsButton = new QnImageButtonWidget(m_headerWidget);
    settingsButton->setIcon(qnSkin->icon("item/close.png"));
    settingsButton->setToolTip(tr("Hide all"));
    settingsButton->setMinimumSize(QSizeF(24, 24));
    settingsButton->setMaximumSize(QSizeF(24, 24));

    QGraphicsLinearLayout *controlsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    controlsLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    controlsLayout->addStretch();
    controlsLayout->addItem(settingsButton);
    controlsLayout->addItem(hideAllButton);
    m_headerWidget->setLayout(controlsLayout);

    QGraphicsLinearLayout* layout = new QGraphicsLinearLayout(Qt::Vertical);
    layout->setContentsMargins(0.5, 0.5, 0.5, 0.5);
    layout->setSpacing(0.5);
    layout->addItem(m_headerWidget);

    m_list = new QnNotificationListWidget(this);
    layout->addItem(m_list);
    layout->setStretchFactor(m_list, 1.0);

    connect(m_list, SIGNAL(itemRemoved(QnNotificationItem*)), this, SLOT(at_list_itemRemoved(QnNotificationItem*)));

    setLayout(layout);
}

QnNotificationsCollectionItem::~QnNotificationsCollectionItem() {
}

QRectF QnNotificationsCollectionItem::headerGeometry() const {
    return m_headerWidget->geometry();
}

void QnNotificationsCollectionItem::showBusinessAction(const QnAbstractBusinessActionPtr &businessAction) {
    QnNotificationItem *item = new QnNotificationItem(m_list);
    item->setText(BusinessActionType::toString(businessAction->actionType()));
    m_list->addItem(item, true);
}

void QnNotificationsCollectionItem::showSystemHealthEvent(QnSystemHealth::MessageType message, const QnResourceList &resources) {
    QnNotificationItem *item = new QnNotificationItem(m_list);
    item->setText(QnSystemHealth::toString(message));
    m_list->addItem(item, true);
}

void QnNotificationsCollectionItem::hideAll() {
    m_list->clear();
}

void QnNotificationsCollectionItem::at_list_itemRemoved(QnNotificationItem *item) {
    delete item;
}
