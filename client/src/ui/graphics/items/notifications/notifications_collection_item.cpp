#include "notifications_collection_item.h"

#include <QtGui/QGraphicsLinearLayout>

#include <core/resource/resource.h>
#include <core/resource_managment/resource_pool.h>

#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/notifications/notification_item.h>
#include <ui/graphics/items/notifications/notification_list_widget.h>
#include <ui/style/skin.h>
#include <ui/style/resource_icon_cache.h>

QnNotificationsCollectionItem::QnNotificationsCollectionItem(QGraphicsItem *parent, Qt::WindowFlags flags) :
    base_type(parent, flags)
{
    m_headerWidget = new QGraphicsWidget(this);

    QnImageButtonWidget* hideAllButton = new QnImageButtonWidget(m_headerWidget);
    hideAllButton->setIcon(qnSkin->icon("titlebar/exit.png"));
    hideAllButton->setToolTip(tr("Hide all"));
    hideAllButton->setMinimumSize(QSizeF(24, 24));
    hideAllButton->setMaximumSize(QSizeF(24, 24));
    connect(hideAllButton, SIGNAL(clicked()), this, SLOT(hideAll()));

    QnImageButtonWidget* settingsButton = new QnImageButtonWidget(m_headerWidget);
    settingsButton->setIcon(qnSkin->icon("titlebar/connected.png"));
    settingsButton->setToolTip(tr(""));
    settingsButton->setMinimumSize(QSizeF(24, 24));
    settingsButton->setMaximumSize(QSizeF(24, 24));
    connect(settingsButton, SIGNAL(clicked()), this, SIGNAL(settingsRequested()));

    QGraphicsLinearLayout *controlsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    controlsLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    controlsLayout->addStretch();
    controlsLayout->addItem(hideAllButton);
    controlsLayout->addItem(settingsButton);
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


    QnBusinessEventParameters params = businessAction->getRuntimeParams();
    BusinessEventType::Value eventType = params.getEventType();

    QString text = BusinessEventType::toString(eventType);


    int resourceId = params.getEventResourceId();
    QnResourcePtr resource = qnResPool->getResourceById(resourceId, QnResourcePool::rfAllResources);
    if (resource) {
        text += QLatin1Char('\n');
        text += resource->getName();

        item->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));
    }
    item->setText(text);


    m_list->addItem(item);
}

void QnNotificationsCollectionItem::showSystemHealthEvent(QnSystemHealth::MessageType message, const QnResourceList &resources) {
    QnNotificationItem *item = new QnNotificationItem(m_list);
    item->setText(QnSystemHealth::toString(message));
    switch (message) {
    case QnSystemHealth::EmailIsEmpty:
        item->setIcon(qnResIconCache->icon(QnResourceIconCache::User));
        break;
    case QnSystemHealth::NoLicenses:
        item->setIcon(qnResIconCache->icon(QnResourceIconCache::Recorder));
        break;
    case QnSystemHealth::SmtpIsNotSet:
        item->setIcon(qnResIconCache->icon(QnResourceIconCache::Server));
        break;
    case QnSystemHealth::UsersEmailIsEmpty:
        item->setIcon(qnResIconCache->icon(QnResourceIconCache::Users));
        break;
    case QnSystemHealth::ConnectionLost:
        item->setIcon(qnSkin->icon("titlebar/disconnected.png"));
        break;
    case QnSystemHealth::EmailSendError:
        item->setIcon(qnResIconCache->icon(QnResourceIconCache::Server | QnResourceIconCache::Offline));
        break;
    case QnSystemHealth::StoragesNotConfigured:
        item->setIcon(qnResIconCache->icon(QnResourceIconCache::Servers));
        break;
    case QnSystemHealth::StoragesAreFull:
        item->setIcon(qnResIconCache->icon(QnResourceIconCache::Servers));
        break;
    default:
        break;
    }

    m_list->addItem(item, true);
}

void QnNotificationsCollectionItem::hideAll() {
    m_list->clear();
}

void QnNotificationsCollectionItem::at_list_itemRemoved(QnNotificationItem *item) {
    delete item;
}
