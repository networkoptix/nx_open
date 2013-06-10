#include "notifications_collection_widget.h"

#include <QtGui/QGraphicsLinearLayout>

#include <business/business_strings_helper.h>

#include <camera/single_thumbnail_loader.h>

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
#include <ui/workbench/handlers/workbench_notifications_handler.h>

//TODO: #GDM remove debug
#include <business/actions/common_business_action.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>

namespace {

    const int buttonSize = 24;
    const int thumbnailHeight = 100;

} //anonymous namespace

QnNotificationsCollectionWidget::QnNotificationsCollectionWidget(QGraphicsItem *parent, Qt::WindowFlags flags, QnWorkbenchContext* context) :
    base_type(parent, flags),
    QnWorkbenchContextAware(context)
{
    m_headerWidget = new GraphicsWidget(this);

    QnImageButtonWidget* hideAllButton = new QnImageButtonWidget(m_headerWidget);
    hideAllButton->setIcon(qnSkin->icon("titlebar/exit.png"));
    hideAllButton->setToolTip(tr("Hide all"));
    hideAllButton->setFixedSize(buttonSize);
    connect(hideAllButton, SIGNAL(clicked()), this, SLOT(hideAll()));

    QnImageButtonWidget* settingsButton = new QnImageButtonWidget(m_headerWidget);
    settingsButton->setIcon(qnSkin->icon("titlebar/connected.png"));
    settingsButton->setToolTip(tr("Settings"));
    settingsButton->setFixedSize(buttonSize);
    connect(settingsButton, SIGNAL(clicked()), this, SLOT(at_settingsButton_clicked()));

    QnImageButtonWidget* eventLogButton = new QnImageButtonWidget(m_headerWidget);
    eventLogButton->setIcon(qnSkin->icon("item/info.png"));
    eventLogButton->setToolTip(tr("Event Log"));
    eventLogButton->setFixedSize(buttonSize);
    connect(eventLogButton, SIGNAL(clicked()), this, SLOT(at_eventLogButton_clicked()));

    QnImageButtonWidget* debugButton = new QnImageButtonWidget(m_headerWidget);
    debugButton->setIcon(qnSkin->icon("item/search.png"));
    debugButton->setToolTip(tr("DEBUG"));
    debugButton->setFixedSize(buttonSize);
    connect(debugButton, SIGNAL(clicked()), this, SLOT(at_debugButton_clicked()));

    QGraphicsLinearLayout *controlsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    controlsLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    controlsLayout->addStretch();
    controlsLayout->addItem(eventLogButton);
    controlsLayout->addItem(hideAllButton);
    controlsLayout->addItem(settingsButton);
    controlsLayout->addItem(debugButton);
    m_headerWidget->setLayout(controlsLayout);

    QGraphicsLinearLayout* layout = new QGraphicsLinearLayout(Qt::Vertical);
    layout->setContentsMargins(1.0, 1.0, 1.0, 1.0);
    layout->setSpacing(0.0);
    layout->addItem(m_headerWidget);

    m_list = new QnNotificationListWidget(this);
    layout->addItem(m_list);

    connect(m_list, SIGNAL(itemRemoved(QnNotificationItem*)), this, SLOT(at_list_itemRemoved(QnNotificationItem*)));
    connect(m_list, SIGNAL(visibleSizeChanged()), this, SIGNAL(visibleSizeChanged()));
    connect(m_list, SIGNAL(sizeHintChanged()), this, SIGNAL(sizeHintChanged()));

    setLayout(layout);

    QnWorkbenchNotificationsHandler* handler = this->context()->instance<QnWorkbenchNotificationsHandler>();
    connect(handler, SIGNAL(businessActionAdded(QnAbstractBusinessActionPtr)),
            this, SLOT(showBusinessAction(QnAbstractBusinessActionPtr)));
    connect(handler, SIGNAL(systemHealthEventAdded(QnSystemHealth::MessageType,const QnResourcePtr&)),
            this, SLOT(showSystemHealthEvent(QnSystemHealth::MessageType,const QnResourcePtr&)));
    connect(handler, SIGNAL(cleared()),
            this, SLOT(hideAll()));
}

QnNotificationsCollectionWidget::~QnNotificationsCollectionWidget() {
}

QRectF QnNotificationsCollectionWidget::headerGeometry() const {
    return m_headerWidget->geometry();
}

QRectF QnNotificationsCollectionWidget::visibleGeometry() const {
    return m_headerWidget->geometry().adjusted(0, 0, 0, m_list->visibleSize().height());
}

void QnNotificationsCollectionWidget::loadThumbnailForItem(QnNotificationItem *item, QnResourcePtr resource, qint64 usecsSinceEpoch)
{
    QnSingleThumbnailLoader* loader = QnSingleThumbnailLoader::newInstance(resource, this);
    connect(loader, SIGNAL(success(QImage)), item, SLOT(setImage(QImage)));
    connect(loader, SIGNAL(finished()), loader, SLOT(deleteLater()));
    loader->load(usecsSinceEpoch, QSize(0, thumbnailHeight)); //width is auto-calculated
}


void QnNotificationsCollectionWidget::showBusinessAction(const QnAbstractBusinessActionPtr &businessAction) {
    QnBusinessEventParameters params = businessAction->getRuntimeParams();
    int resourceId = params.getEventResourceId();
    QnResourcePtr resource = qnResPool->getResourceById(resourceId, QnResourcePool::rfAllResources);
    if (!resource)
        return;

    QnNotificationItem *item = new QnNotificationItem(m_list);

    item->setText(QnBusinessStringsHelper::shortEventDescription(params));
    item->setTooltipText(QnBusinessStringsHelper::longEventDescription(businessAction));

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
                        .withArgument(Qn::ItemTimeRole, params.getEventTimestamp()/1000),
                        2.0, true
                        );
//            loadThumbnailForItem(item, resource, params.getEventTimestamp());
            loadThumbnailForItem(item, resource); //TODO: #GDM loading latest while Roma fixes mediaserver
            break;
        }

    case BusinessEventType::Camera_Input: {
            item->setColor(qnGlobals->notificationColorCommon());
            item->addActionButton(
                        qnResIconCache->icon(resource->flags(), resource->getStatus()),
                        tr("Open Camera"),
                        Qn::OpenInNewLayoutAction,
                        QnActionParameters(resource),
                        2.0, true
                        );
            loadThumbnailForItem(item, resource);
            break;
        }
    case BusinessEventType::Camera_Disconnect: {
            item->setColor(qnGlobals->notificationColorImportant());
            item->addActionButton(
                        qnResIconCache->icon(resource->flags(), resource->getStatus()),
                        tr("Open Camera"),
                        Qn::OpenInNewLayoutAction,
                        QnActionParameters(resource),
                        2.0, true
                        );
            item->addActionButton(
                        qnResIconCache->icon(resource->flags(), resource->getStatus()),
                        tr("Camera Settings"),
                        Qn::CameraSettingsAction,
                        QnActionParameters(resource)
                        );
            loadThumbnailForItem(item, resource);
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
            break;
        }
    case BusinessEventType::Network_Issue:{
            item->setColor(qnGlobals->notificationColorImportant());
            item->addActionButton(
                        qnResIconCache->icon(resource->flags(), resource->getStatus()),
                        tr("Open Camera"),
                        Qn::OpenInNewLayoutAction,
                        QnActionParameters(resource),
                        2.0, true
                        );
            item->addActionButton(
                        qnResIconCache->icon(resource->flags(), resource->getStatus()),
                        tr("Camera Settings"),
                        Qn::CameraSettingsAction,
                        QnActionParameters(resource)
                        );
            loadThumbnailForItem(item, resource);
            break;
        }

    case BusinessEventType::Camera_Ip_Conflict: {
            item->setColor(qnGlobals->notificationColorCritical());
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
            break;
        }
    default:
        break;
    }

    connect(item, SIGNAL(actionTriggered(Qn::ActionId, const QnActionParameters&)), this, SLOT(at_item_actionTriggered(Qn::ActionId, const QnActionParameters&)));
    m_list->addItem(item);
}

void QnNotificationsCollectionWidget::showSystemHealthEvent(QnSystemHealth::MessageType message, const QnResourcePtr &resource) {
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
        item->setText(tr("E-Mail address is not set for user %1.").arg(name));
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
        item->setText(tr("Storages are not configured on %1.").arg(name));
        break;
    case QnSystemHealth::StoragesAreFull:
        item->addActionButton(
                    qnResIconCache->icon(resource->flags(), resource->getStatus()),
                    tr("Server settings"),
                    Qn::ServerSettingsAction,
                    QnActionParameters(resource)
                    );
        item->setText(tr("Some storages are full on %1.").arg(name));
        break;
    default:
        break;
    }
    if (item->text().isEmpty()) {
        QString text = QnSystemHealth::messageName(message);
        item->setText(text);
    }
    item->setTooltipText(QnSystemHealth::messageDescription(message));

    connect(item, SIGNAL(actionTriggered(Qn::ActionId, const QnActionParameters&)), this, SLOT(at_item_actionTriggered(Qn::ActionId, const QnActionParameters&)));

    m_list->addItem(item, message != QnSystemHealth::ConnectionLost);
}

void QnNotificationsCollectionWidget::hideAll() {
    m_list->clear();
}

void QnNotificationsCollectionWidget::at_settingsButton_clicked() {
    menu()->trigger(Qn::OpenPopupSettingsAction);
}

void QnNotificationsCollectionWidget::at_eventLogButton_clicked() {
    menu()->trigger(Qn::BusinessEventsLogAction);
}

void QnNotificationsCollectionWidget::at_debugButton_clicked() {

    QnResourceList servers = qnResPool->getResources().filtered<QnMediaServerResource>();
    QnResourcePtr sampleServer = servers.isEmpty() ? QnResourcePtr() : servers.first();

    QnResourceList cameras = qnResPool->getResources().filtered<QnVirtualCameraResource>();
    QnResourcePtr sampleCamera = cameras.isEmpty() ? QnResourcePtr() : cameras.first();

    //TODO: #GDM REMOVE DEBUG
    for (int i = 0; i < QnSystemHealth::MessageTypeCount; i++) {
        QnSystemHealth::MessageType message = QnSystemHealth::MessageType(i);
        QnResourcePtr resource;
        switch (message) {
        case QnSystemHealth::EmailIsEmpty:
            resource = context()->user();
            break;
        case QnSystemHealth::UsersEmailIsEmpty:
            resource = qnResPool->getResources().filtered<QnUserResource>().last();
            break;
        case QnSystemHealth::StoragesNotConfigured:
        case QnSystemHealth::StoragesAreFull:
            resource = sampleServer;
            break;
        default:
            break;
        }
        showSystemHealthEvent(message, resource);
    }

    //TODO: #GDM REMOVE DEBUG
    for (int i = 0; i < BusinessEventType::Count; i++) {
        BusinessEventType::Value eventType = BusinessEventType::Value(i);

        QnBusinessEventParameters params;
        params.setEventType(eventType);
        switch(eventType) {
        case BusinessEventType::Camera_Motion:
        case BusinessEventType::Camera_Input:
        case BusinessEventType::Camera_Disconnect:
        case BusinessEventType::Network_Issue:
            params.setEventResourceId(sampleCamera->getId());
            break;

        case BusinessEventType::Storage_Failure:
        case BusinessEventType::Camera_Ip_Conflict:
        case BusinessEventType::MediaServer_Failure:
        case BusinessEventType::MediaServer_Conflict:
            params.setEventResourceId(sampleServer->getId());
            break;
        default:
            break;

        }

        QnAbstractBusinessActionPtr baction(new QnCommonBusinessAction(BusinessActionType::ShowPopup, params));
        showBusinessAction(baction);
    }
}

void QnNotificationsCollectionWidget::at_list_itemRemoved(QnNotificationItem *item) {
    delete item;
}

void QnNotificationsCollectionWidget::at_item_actionTriggered(Qn::ActionId actionId, const QnActionParameters &parameters) {
    menu()->trigger(actionId, parameters);
}

