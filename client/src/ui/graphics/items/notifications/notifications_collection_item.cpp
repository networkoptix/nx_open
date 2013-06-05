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
#include <ui/style/globals.h>
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
    settingsButton->setToolTip(tr("Settings"));
    settingsButton->setMinimumSize(QSizeF(24, 24));
    settingsButton->setMaximumSize(QSizeF(24, 24));
    connect(settingsButton, SIGNAL(clicked()), this, SLOT(at_settingsButton_clicked()));

    QnImageButtonWidget* eventLogButton = new QnImageButtonWidget(m_headerWidget);
    eventLogButton->setIcon(qnSkin->icon("item/info.png"));
    eventLogButton->setToolTip(tr("Event Log"));
    eventLogButton->setMinimumSize(QSizeF(24, 24));
    eventLogButton->setMaximumSize(QSizeF(24, 24));
    connect(eventLogButton, SIGNAL(clicked()), this, SLOT(at_eventLogButton_clicked()));

    QGraphicsLinearLayout *controlsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    controlsLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    controlsLayout->addStretch();
    controlsLayout->addItem(eventLogButton);
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

/*
    int eventReasonCode = QnBusinessEventRuntime::getReasonCode(eventParams);

    item->setText(getResourceName(resource));
    item->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));
    item->setData(QnBusinessEventRuntime::getEventResourceId(eventParams), ResourceIdRole);
    item->setData(0, EventCountRole);
    item->setData(getEventTime(eventParams), EventTimeRole);
    item->setData(eventReasonCode, EventReasonRole);
    item->setData(QnBusinessEventRuntime::getReasonText(eventParams), EventReasonTextRole);



 *
QnBusiness::EventReason reasonCode = (QnBusiness::EventReason)item->data(EventReasonRole).toInt();
    QString reasonText = item->data(EventReasonTextRole).toString();

    switch (reasonCode) {
        case QnBusiness::NetworkIssueNoFrame:
            if (m_eventType == BusinessEventType::Network_Issue)
                item->appendRow(new QStandardItem(tr("No video frame received\nduring last %1 seconds.")
                                                  .arg(reasonText)));
            break;
        case QnBusiness::NetworkIssueConnectionClosed:
            if (m_eventType == BusinessEventType::Network_Issue)
                item->appendRow(new QStandardItem(tr("Connection to camera\nwas unexpectedly closed")));
            break;
        case QnBusiness::NetworkIssueRtpPacketLoss:
            if (m_eventType == BusinessEventType::Network_Issue) {
                QStringList seqs = reasonText.split(QLatin1Char(';'));
                if (seqs.size() != 2)
                    break;
                QString text = tr("RTP packet loss detected.\nPrev seq.=%1 next seq.=%2")
                        .arg(seqs[0])
                        .arg(seqs[1]);
                item->appendRow(new QStandardItem(text));
            }
            break;
        case QnBusiness::MServerIssueTerminated:
            if (m_eventType == BusinessEventType::MediaServer_Failure)
                item->appendRow(new QStandardItem(tr("Server terminated.")));
            break;
        case QnBusiness::MServerIssueStarted:
            if (m_eventType == BusinessEventType::MediaServer_Failure)
                item->appendRow(new QStandardItem(tr("Server started after crash.")));
            break;
        case QnBusiness::StorageIssueIoError:
            if (m_eventType == BusinessEventType::Storage_Failure)
                item->appendRow(new QStandardItem(tr("I/O Error occured at\n%1")
                                                  .arg(reasonText)));
            break;
        case QnBusiness::StorageIssueNotEnoughSpeed:
            if (m_eventType == BusinessEventType::Storage_Failure)
                item->appendRow(new QStandardItem(tr("Not enough HDD/SSD speed\nfor recording at\n%1.")
                                                  .arg(reasonText)));
            break;
        default:
            break;
    }
 **/

void QnNotificationsCollectionItem::showBusinessAction(const QnAbstractBusinessActionPtr &businessAction) {
    QnBusinessEventParameters params = businessAction->getRuntimeParams();
    int resourceId = params.getEventResourceId();
    QnResourcePtr resource = qnResPool->getResourceById(resourceId, QnResourcePool::rfAllResources);
    if (!resource)
        return;

    QnNotificationItem *item = new QnNotificationItem(m_list);
    QString name = resource->getName();

    BusinessEventType::Value eventType = params.getEventType();

    switch (eventType) {
    case BusinessEventType::Camera_Motion: {
            item->setColor(qnGlobals->notificationColorCommon());
            item->addActionButton(
                        qnResIconCache->icon(resource->flags(), resource->getStatus()),
                        tr("Browse Archive"),
                        Qn::OpenInNewLayoutAction,
                        QnActionParameters(resource)
                        .withArgument(Qn::ItemTimeRole, params.getEventTimestamp()/1000)
                        );
            item->setText(tr("Motion was detected on camera %1.").arg(name));
            break;
        }

    case BusinessEventType::Camera_Input: {
            item->setColor(qnGlobals->notificationColorCommon());
            item->addActionButton(
                        qnResIconCache->icon(resource->flags(), resource->getStatus()),
                        tr("Open Camera"),
                        Qn::OpenInNewLayoutAction,
                        QnActionParameters(resource)
                        );
            item->setText(tr("Input signal was detected on camera %1.").arg(name));
            break;
        }
    case BusinessEventType::Camera_Disconnect: {
            item->setColor(qnGlobals->notificationColorImportant());
            item->addActionButton(
                        qnResIconCache->icon(resource->flags(), resource->getStatus()),
                        tr("Open Camera"),
                        Qn::OpenInNewLayoutAction,
                        QnActionParameters(resource)
                        );
            item->addActionButton(
                        qnResIconCache->icon(resource->flags(), resource->getStatus()),
                        tr("Camera Settings"),
                        Qn::CameraSettingsAction,
                        QnActionParameters(resource)
                        );
            item->setText(tr("Camera %1 disconnected.").arg(name));
            break;
        }

    case BusinessEventType::Storage_Failure: {
            item->setColor(qnGlobals->notificationColorImportant());
            item->addActionButton(
                        qnResIconCache->icon(resource->flags(), resource->getStatus()),
                        tr("Open Monitor"),
                        Qn::OpenInNewLayoutAction,
                        QnActionParameters(resource)
                        );
            item->addActionButton(
                        qnResIconCache->icon(resource->flags(), resource->getStatus()),
                        tr("Server settings"),
                        Qn::ServerSettingsAction,
                        QnActionParameters(resource)
                        );
            item->setText(tr("Storage failure on %1.").arg(name));
            break;
        }
    case BusinessEventType::Network_Issue:{
            item->setColor(qnGlobals->notificationColorImportant());
            item->addActionButton(
                        qnResIconCache->icon(resource->flags(), resource->getStatus()),
                        tr("Open Camera"),
                        Qn::OpenInNewLayoutAction,
                        QnActionParameters(resource)
                        );
            item->addActionButton(
                        qnResIconCache->icon(resource->flags(), resource->getStatus()),
                        tr("Camera Settings"),
                        Qn::CameraSettingsAction,
                        QnActionParameters(resource)
                        );
            item->setText(tr("Network issue on %1.").arg(name));
            break;
        }

    case BusinessEventType::Camera_Ip_Conflict: {
            item->setColor(qnGlobals->notificationColorCritical());
            item->setText(tr("Cameras' IP conflict detected."));
            //TODO: #GDM page in browser
            break;
        }
    case BusinessEventType::MediaServer_Failure: {
            item->setColor(qnGlobals->notificationColorCritical());
            item->addActionButton(
                        qnResIconCache->icon(resource->flags(), resource->getStatus()),
                        tr("Open Monitor"),
                        Qn::OpenInNewLayoutAction,
                        QnActionParameters(resource)
                        );
            // TODO: #GDM second action : ping
            item->setText(tr("Failure on %1.").arg(name));
            break;
        }
    case BusinessEventType::MediaServer_Conflict: {
            item->setColor(qnGlobals->notificationColorCritical());
            //TODO: #GDM notification

            item->setText(tr("Media Servers' conflict detected."));
            break;
        }

    default:
        break;
    }

    connect(item, SIGNAL(actionTriggered(Qn::ActionId, const QnActionParameters&)), this, SLOT(at_item_actionTriggered(Qn::ActionId, const QnActionParameters&)));
    m_list->addItem(item);
}

void QnNotificationsCollectionItem::showSystemHealthEvent(QnSystemHealth::MessageType message, const QnResourcePtr &resource) {
    QnNotificationItem *item = new QnNotificationItem(m_list);

    QString name = resource ? resource->getName() : QString();

    item->setColor(qnGlobals->notificationColorSystem());

    switch (message) {
    case QnSystemHealth::EmailIsEmpty:
        item->addActionButton(
                    qnResIconCache->icon(QnResourceIconCache::User),
                    tr("User Settings"),
                    Qn::UserSettingsAction,
                    QnActionParameters(context()->user())
                    .withArgument(Qn::FocusElementRole, QString(QLatin1String("email")))
                    );
        //default text
        break;
    case QnSystemHealth::NoLicenses:
        item->addActionButton(
                    qnResIconCache->icon(QnResourceIconCache::Servers),
                    tr("Licenses"),
                    Qn::GetMoreLicensesAction
                    );
        //default text
        break;
    case QnSystemHealth::SmtpIsNotSet:
        item->addActionButton(
                    qnResIconCache->icon(QnResourceIconCache::Servers),
                    tr("SMTP Settings"),
                    Qn::OpenServerSettingsAction
                    );
        //default text
        break;
    case QnSystemHealth::UsersEmailIsEmpty:
        item->addActionButton(
                    qnResIconCache->icon(QnResourceIconCache::User),
                    tr("User Settings"),
                    Qn::UserSettingsAction,
                    QnActionParameters(resource)
                    .withArgument(Qn::FocusElementRole, QString(QLatin1String("email")))
                    );
        item->setText(tr("E-Mail address is not set for user %1").arg(name));
        break;
    case QnSystemHealth::ConnectionLost:
        item->addActionButton(
                    qnSkin->icon("titlebar/disconnected.png"),
                    tr("Connect to server"),
                    Qn::ConnectToServerAction
                    );
        //default text
        break;
    case QnSystemHealth::EmailSendError:
        item->addActionButton(
                    qnResIconCache->icon(QnResourceIconCache::Servers),
                    tr("SMTP Settings"),
                    Qn::OpenServerSettingsAction
                    );
        //default text
        break;
    case QnSystemHealth::StoragesNotConfigured:
        item->addActionButton(
                    qnResIconCache->icon(resource->flags(), resource->getStatus()),
                    tr("Server settings"),
                    Qn::ServerSettingsAction,
                    QnActionParameters(resource)
                    );
        item->setText(tr("Storages are not configured on %1").arg(name));
        break;
    case QnSystemHealth::StoragesAreFull:
        item->addActionButton(
                    qnResIconCache->icon(resource->flags(), resource->getStatus()),
                    tr("Server settings"),
                    Qn::ServerSettingsAction,
                    QnActionParameters(resource)
                    );
        item->setText(tr("Some storages are full on %1").arg(name));
        break;
    default:
        break;
    }
    if (item->text().isEmpty()) {
        QString text = QnSystemHealth::toString(message, true);
        item->setText(text);
    }

    connect(item, SIGNAL(actionTriggered(Qn::ActionId, const QnActionParameters&)), this, SLOT(at_item_actionTriggered(Qn::ActionId, const QnActionParameters&)));

    m_list->addItem(item, message != QnSystemHealth::ConnectionLost);
}

void QnNotificationsCollectionItem::hideAll() {
    m_list->clear();
}

void QnNotificationsCollectionItem::at_settingsButton_clicked() {
    menu()->trigger(Qn::OpenPopupSettingsAction);
}

void QnNotificationsCollectionItem::at_eventLogButton_clicked() {
    menu()->trigger(Qn::BusinessEventsLogAction);
}

void QnNotificationsCollectionItem::at_list_itemRemoved(QnNotificationItem *item) {
    delete item;
}

void QnNotificationsCollectionItem::at_item_actionTriggered(Qn::ActionId actionId, const QnActionParameters &parameters) {
    menu()->trigger(actionId, parameters);
}
