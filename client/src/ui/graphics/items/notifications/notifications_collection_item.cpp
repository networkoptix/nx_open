#include "notifications_collection_item.h"

#include <QtGui/QGraphicsLinearLayout>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/notifications/notification_item.h>
#include <ui/graphics/items/notifications/notification_list_widget.h>
#include <ui/style/skin.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>

QnNotificationsCollectionItem::QnNotificationsCollectionItem(QGraphicsItem *parent, Qt::WindowFlags flags, QnWorkbenchContext* context) :
    base_type(parent, flags),
    QnWorkbenchContextAware(context)
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
    QnBusinessEventParameters params = businessAction->getRuntimeParams();
    int resourceId = params.getEventResourceId();
    QnResourcePtr resource = qnResPool->getResourceById(resourceId, QnResourcePool::rfAllResources);
    if (!resource)
        return;

    QnNotificationItem *item = new QnNotificationItem(m_list);


    BusinessEventType::Value eventType = params.getEventType();

    switch (eventType) {
    case BusinessEventType::Camera_Input:                   //camera
    case BusinessEventType::Camera_Disconnect:              //camera
        item->setColor(QColor(255, 131, 48)); //orange
        break;

    case BusinessEventType::Storage_Failure:                //server -- monitor
    case BusinessEventType::Network_Issue:                  //camera
    case BusinessEventType::Camera_Ip_Conflict:             //server - mac addrs in conflict list
    case BusinessEventType::MediaServer_Failure:            //server -- monitor
    case BusinessEventType::MediaServer_Conflict:           //server -- monitor
        item->setColor(QColor(237, 200, 66)); // yellow
        break;

    case BusinessEventType::Camera_Motion:                  //camera
        item->setColor(QColor(103, 237, 66)); // green
        break;

    default:
        break;
    }

    m_actionDataByItem.insert(item, ActionData(Qn::OpenInNewLayoutAction,
                                               QnActionParameters(resource)));

    QString text = BusinessEventType::toString(eventType);
    text += QLatin1Char('\n');
    text += resource->getName();
    item->setText(text);
    item->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));

    connect(item, SIGNAL(actionTriggered(QnNotificationItem*)), this, SLOT(at_item_actionTriggered(QnNotificationItem*)));
    m_list->addItem(item);
}

void QnNotificationsCollectionItem::showSystemHealthEvent(QnSystemHealth::MessageType message, const QnResourcePtr &resource) {
    QnNotificationItem *item = new QnNotificationItem(m_list);
    QString text = QnSystemHealth::toString(message);
    if (resource) {
        text += QLatin1Char('\n');
        text += resource->getName();
    }
    item->setText(text);

    item->setColor(QColor(255, 0, 0)); // red

    switch (message) {
    case QnSystemHealth::EmailIsEmpty:
        item->setIcon(qnResIconCache->icon(QnResourceIconCache::User));
        m_actionDataByItem.insert(item, ActionData(Qn::UserSettingsAction,
                                                   QnActionParameters(context()->user())
                                                   .withArgument(Qn::FocusElementRole, QString(QLatin1String("email")))));

        break;
    case QnSystemHealth::NoLicenses:
        item->setIcon(qnResIconCache->icon(QnResourceIconCache::Servers));
        m_actionDataByItem.insert(item, ActionData(Qn::GetMoreLicensesAction));
        break;
    case QnSystemHealth::SmtpIsNotSet:
        item->setIcon(qnResIconCache->icon(QnResourceIconCache::Servers));
        m_actionDataByItem.insert(item, ActionData(Qn::OpenServerSettingsAction));
        break;
    case QnSystemHealth::UsersEmailIsEmpty:
        item->setIcon(qnResIconCache->icon(QnResourceIconCache::Users));
        m_actionDataByItem.insert(item, ActionData(Qn::UserSettingsAction,
                                                   QnActionParameters(resource)
                                                   .withArgument(Qn::FocusElementRole, QString(QLatin1String("email")))));
        break;
    case QnSystemHealth::ConnectionLost:
        item->setIcon(qnSkin->icon("titlebar/disconnected.png"));
        m_actionDataByItem.insert(item, ActionData(Qn::ConnectToServerAction));
        break;
    case QnSystemHealth::EmailSendError:
        item->setIcon(qnResIconCache->icon(QnResourceIconCache::Servers));
        m_actionDataByItem.insert(item, ActionData(Qn::OpenServerSettingsAction));
        break;
    case QnSystemHealth::StoragesNotConfigured:
        item->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));
        m_actionDataByItem.insert(item, ActionData(Qn::ServerSettingsAction,
                                                   QnActionParameters(resource)));
        break;
    case QnSystemHealth::StoragesAreFull:
        item->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));
        m_actionDataByItem.insert(item, ActionData(Qn::ServerSettingsAction,
                                                   QnActionParameters(resource)));
        break;
    default:
        break;
    }
    connect(item, SIGNAL(actionTriggered(QnNotificationItem*)), this, SLOT(at_item_actionTriggered(QnNotificationItem*)));

    m_list->addItem(item, message != QnSystemHealth::ConnectionLost);
}

void QnNotificationsCollectionItem::hideAll() {
    m_list->clear();
}

void QnNotificationsCollectionItem::at_list_itemRemoved(QnNotificationItem *item) {
    m_actionDataByItem.remove(item);
    delete item;
}

void QnNotificationsCollectionItem::at_item_actionTriggered(QnNotificationItem* item) {
    if (!m_actionDataByItem.contains(item))
        return;

    ActionData data = m_actionDataByItem[item];
    menu()->trigger(data.action, data.params);
}
