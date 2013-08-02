#include "notifications_collection_widget.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsLinearLayout>

#include <utils/common/delete_later.h>

#include <business/business_strings_helper.h>

#include <camera/single_thumbnail_loader.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <client/client_settings.h>

#include <ui/animation/opacity_animator.h>
#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/common/geometry.h>
#include <ui/common/ui_resource_name.h>
#include <ui/common/notification_levels.h>
#include <ui/graphics/items/generic/particle_item.h>
#include <ui/graphics/items/generic/tool_tip_widget.h>
#include <ui/graphics/items/notifications/notification_item.h>
#include <ui/graphics/items/notifications/notification_list_widget.h>
#include <ui/style/skin.h>
#include <ui/style/globals.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/handlers/workbench_notifications_handler.h>

#include <utils/math/color_transformations.h>

//TODO: #GDM remove debug
#include <business/actions/common_business_action.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>

namespace {
    const qreal widgetHeight = 24;
    const int thumbnailHeight = 100;

    /** We limit the maximal number of notification items to prevent crashes due
     * to reaching GDI resource limit. */
    const int maxNotificationItems = 128;


    const char *itemResourcePropertyName = "_qn_itemResource";

} //anonymous namespace

QnBlinkingImageButtonWidget::QnBlinkingImageButtonWidget(QGraphicsItem *parent):
    base_type(parent),
    m_time(0),
    m_count(0)
{
    registerAnimation(this);
    startListening();

    m_particle = new QnParticleItem(this);

    m_balloon = new QnToolTipWidget(this);
    m_balloon->setText(tr("You have new notifications"));
    m_balloon->setOpacity(0.0);

    connect(m_balloon,  SIGNAL(geometryChanged()),  this, SLOT(updateBalloonTailPos()));
    connect(this,       SIGNAL(geometryChanged()),  this, SLOT(updateParticleGeometry()));
    connect(this,       SIGNAL(toggled(bool)),      this, SLOT(updateParticleVisibility()));
    connect(m_particle, SIGNAL(visibleChanged()),   this, SLOT(at_particle_visibleChanged()));

    updateParticleGeometry();
    updateParticleVisibility();
    updateToolTip();
    updateBalloonTailPos();
}

void QnBlinkingImageButtonWidget::setNotificationCount(int count) {
    if(m_count == count)
        return;

    m_count = count;

    updateParticleVisibility();
    updateToolTip();
}

void QnBlinkingImageButtonWidget::setColor(const QColor &color) {
    m_particle->setColor(color);
}

void QnBlinkingImageButtonWidget::showBalloon() {
    /*updateBalloonGeometry();
    opacityAnimator(m_balloon, 4.0)->animateTo(1.0);

    QTimer::singleShot(3000, this, SLOT(hideBalloon()));*/
}

void QnBlinkingImageButtonWidget::hideBalloon() {
    /*opacityAnimator(m_balloon, 4.0)->animateTo(0.0);*/
}

void QnBlinkingImageButtonWidget::updateParticleGeometry() {
    QRectF rect = this->rect();
    qreal radius = (rect.width() + rect.height()) / 4.0;

    m_particle->setRect(QRectF(rect.center() - QPointF(radius, radius), 2.0 * QSizeF(radius, radius)));
}

void QnBlinkingImageButtonWidget::updateParticleVisibility() {
    m_particle->setVisible(m_count > 0 && !isChecked());
}

void QnBlinkingImageButtonWidget::updateToolTip() {
    setToolTip(tr("You have %n notifications", "", m_count));
}

void QnBlinkingImageButtonWidget::updateBalloonTailPos() {
    QRectF rect = m_balloon->rect();
    m_balloon->setTailPos(QPointF(rect.right() + 8.0, rect.center().y()));
}

void QnBlinkingImageButtonWidget::updateBalloonGeometry() {
    QRectF rect = this->rect();
    m_balloon->pointTo(QPointF(rect.left(), rect.center().y()));
}

void QnBlinkingImageButtonWidget::tick(int deltaMSecs) {
    m_time += deltaMSecs;
    
    m_particle->setOpacity(0.6 + 0.4 * std::sin(m_time / 1000.0 * 2 * M_PI));
}

void QnBlinkingImageButtonWidget::at_particle_visibleChanged() {
    if(m_particle->isVisible())
        showBalloon();
}


// ---------------------- QnNotificationsCollectionWidget -------------------


QnNotificationsCollectionWidget::QnNotificationsCollectionWidget(QGraphicsItem *parent, Qt::WindowFlags flags, QnWorkbenchContext* context) :
    base_type(parent, flags),
    QnWorkbenchContextAware(context)
{
    m_headerWidget = new GraphicsWidget(this);

    qreal buttonSize = QApplication::style()->pixelMetric(QStyle::PM_ToolBarIconSize, NULL, NULL);

    QnImageButtonWidget *settingsButton = new QnImageButtonWidget(m_headerWidget);
    settingsButton->setIcon(qnSkin->icon("events/settings.png"));
    settingsButton->setToolTip(tr("Settings..."));
    settingsButton->setFixedSize(buttonSize);
    settingsButton->setCached(true);
    connect(settingsButton,   SIGNAL(clicked()),
            this->context()->action(Qn::BusinessEventsAction), SIGNAL(triggered()));
//    connect(settingsButton, SIGNAL(clicked()), this, SLOT(at_settingsButton_clicked()));

    QnImageButtonWidget *filterButton = new QnImageButtonWidget(m_headerWidget);
    filterButton->setIcon(qnSkin->icon("events/filter.png"));
    filterButton->setToolTip(tr("Filter..."));
    filterButton->setFixedSize(buttonSize);
    filterButton->setCached(true);
    connect(filterButton,   SIGNAL(clicked()),
            this->context()->action(Qn::PreferencesNotificationTabAction), SIGNAL(triggered()));
    //connect(filterButton, SIGNAL(clicked()), this, SLOT(at_filterButton_clicked()));

    QnImageButtonWidget *eventLogButton = new QnImageButtonWidget(m_headerWidget);
    eventLogButton->setIcon(qnSkin->icon("events/log.png"));
    eventLogButton->setToolTip(tr("Event Log"));
    eventLogButton->setFixedSize(buttonSize);
    eventLogButton->setCached(true);
    connect(eventLogButton,   SIGNAL(clicked()),
            this->context()->action(Qn::BusinessEventsLogAction), SIGNAL(triggered()));
    //connect(eventLogButton, SIGNAL(clicked()), this, SLOT(at_eventLogButton_clicked()));

    QnImageButtonWidget *debugButton = NULL;
    if(qnSettings->isDevMode()) {
        debugButton = new QnImageButtonWidget(m_headerWidget);
        debugButton->setIcon(qnSkin->icon("item/search.png"));
        debugButton->setToolTip(tr("DEBUG"));
        debugButton->setFixedSize(buttonSize);
        debugButton->setCached(true);
        connect(debugButton, SIGNAL(clicked()), this, SLOT(at_debugButton_clicked()));
    }

    qreal margin = (widgetHeight - buttonSize) / 2.0;
    QGraphicsLinearLayout *controlsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    controlsLayout->setSpacing(2.0);
    controlsLayout->setContentsMargins(2.0, margin, 2.0, margin);
    controlsLayout->addStretch();
    if(debugButton)
        controlsLayout->addItem(debugButton);
    controlsLayout->addItem(eventLogButton);
    controlsLayout->addItem(settingsButton);
    controlsLayout->addItem(filterButton);
    m_headerWidget->setLayout(controlsLayout);

    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(Qt::Vertical);
    layout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    layout->setSpacing(0.0);
    layout->addItem(m_headerWidget);

    m_list = new QnNotificationListWidget(this);
    layout->addItem(m_list);

    connect(m_list, SIGNAL(itemRemoved(QnNotificationItem*)), this, SLOT(at_list_itemRemoved(QnNotificationItem*)));
    connect(m_list, SIGNAL(visibleSizeChanged()), this, SIGNAL(visibleSizeChanged()));
    connect(m_list, SIGNAL(sizeHintChanged()), this, SIGNAL(sizeHintChanged()));


    setLayout(layout);

    QnWorkbenchNotificationsHandler *handler = this->context()->instance<QnWorkbenchNotificationsHandler>();
    connect(handler, SIGNAL(businessActionAdded(QnAbstractBusinessActionPtr)),
            this, SLOT(showBusinessAction(QnAbstractBusinessActionPtr)));
    connect(handler,    SIGNAL(systemHealthEventAdded   (QnSystemHealth::MessageType, const QnResourcePtr&)),
            this,       SLOT(showSystemHealthMessage    (QnSystemHealth::MessageType, const QnResourcePtr&)));
    connect(handler,    SIGNAL(systemHealthEventRemoved (QnSystemHealth::MessageType, const QnResourcePtr&)),
            this,       SLOT(hideSystemHealthMessage    (QnSystemHealth::MessageType, const QnResourcePtr&)));
    connect(handler,    SIGNAL(cleared()),
            this,       SLOT(hideAll()));

}

QnNotificationsCollectionWidget::~QnNotificationsCollectionWidget() {
}

QRectF QnNotificationsCollectionWidget::headerGeometry() const {
    return m_headerWidget->geometry();
}

QRectF QnNotificationsCollectionWidget::visibleGeometry() const {
    return m_headerWidget->geometry().adjusted(0, 0, 0, m_list->visibleSize().height());
}

void QnNotificationsCollectionWidget::setToolTipsEnclosingRect(const QRectF &rect) {
    QRectF listRect = rect;
    listRect.setTop(m_list->geometry().topLeft().y());

    m_list->setToolTipsEnclosingRect(mapRectToItem(m_list, listRect));
}

void QnNotificationsCollectionWidget::setBlinker(QnBlinkingImageButtonWidget *blinker) {
    if (m_blinker)
        disconnect(m_list, 0, m_blinker, 0);
    
    m_blinker = blinker;
    
    if (m_blinker) {
        connect(m_list, SIGNAL(itemCountChanged()),         this, SLOT(updateBlinker()));
        connect(m_list, SIGNAL(notificationLevelChanged()), this, SLOT(updateBlinker()));
        updateBlinker();
    }
}

void QnNotificationsCollectionWidget::loadThumbnailForItem(QnNotificationItem *item, QnResourcePtr resource, qint64 usecsSinceEpoch)
{
    QnSingleThumbnailLoader *loader = QnSingleThumbnailLoader::newInstance(resource, usecsSinceEpoch, QSize(0, thumbnailHeight), item);
    item->setImageProvider(loader);
    //connect(loader, SIGNAL(finished()), loader, SLOT(deleteLater()));
}

void QnNotificationsCollectionWidget::showBusinessAction(const QnAbstractBusinessActionPtr &businessAction) {
    QnBusinessEventParameters params = businessAction->getRuntimeParams();
    int resourceId = params.getEventResourceId();
    QnResourcePtr resource = qnResPool->getResourceById(resourceId, QnResourcePool::AllResources);
    if (!resource)
        return;

    if(m_list->itemCount() >= maxNotificationItems)
        return; /* Just drop the notification if we already have too many of them in queue. */

    QnNotificationItem *item = new QnNotificationItem(m_list);

    QString name = getResourceName(resource);
    BusinessEventType::Value eventType = params.getEventType();

    item->setText(QnBusinessStringsHelper::eventAtResource(params, qnSettings->isIpShownInTree()));
    item->setTooltipText(QnBusinessStringsHelper::eventDescription(businessAction, QnBusinessAggregationInfo(), qnSettings->isIpShownInTree(), false));
    item->setNotificationLevel(QnNotificationLevels::notificationLevel(eventType));

    switch (eventType) {
    case BusinessEventType::Camera_Motion: {
        item->addActionButton(
            qnSkin->icon("events/camera.png"),
            tr("Browse Archive"),
            Qn::OpenInNewLayoutAction,
            QnActionParameters(resource).withArgument(Qn::ItemTimeRole, params.getEventTimestamp()/1000)
        );
        loadThumbnailForItem(item, resource, params.getEventTimestamp());
        break;
    }
    case BusinessEventType::Camera_Input: {
        item->addActionButton(
            qnSkin->icon("events/camera.png"),
            tr("Open Camera"),
            Qn::OpenInNewLayoutAction,
            QnActionParameters(resource)
        );
        loadThumbnailForItem(item, resource);
        break;
    }
    case BusinessEventType::Camera_Disconnect: {
        item->addActionButton(
            qnSkin->icon("events/camera.png"),
            tr("Camera Settings"),
            Qn::CameraSettingsAction,
            QnActionParameters(resource)
        );
        loadThumbnailForItem(item, resource);
        break;
    }
    case BusinessEventType::Storage_Failure: {
        item->addActionButton(
            qnSkin->icon("events/storage.png"),
            tr("Server settings"),
            Qn::ServerSettingsAction,
            QnActionParameters(resource)
        );
        break;
    }
    case BusinessEventType::Network_Issue:{
        item->addActionButton(
            qnSkin->icon("events/server.png"),
            tr("Camera Settings"),
            Qn::CameraSettingsAction,
            QnActionParameters(resource)
        );
        loadThumbnailForItem(item, resource);
        break;
    }
    case BusinessEventType::Camera_Ip_Conflict: {
        QString webPageAddress = params.getSource();

        item->addActionButton(
            qnSkin->icon("events/camera.png"),
            tr("Open camera web page..."),
            Qn::BrowseUrlAction,
            QnActionParameters().withArgument(Qn::UrlRole, webPageAddress)
        );
        break;
    }
    case BusinessEventType::MediaServer_Failure: {
        item->addActionButton(
            qnSkin->icon("events/server.png"),
            tr("Settings"),
            Qn::ServerSettingsAction,
            QnActionParameters(resource)
        );
        break;
    }
    case BusinessEventType::MediaServer_Conflict: {
        item->addActionButton(
            qnSkin->icon("events/server.png"),
            QString(),
            Qn::NoAction
        );
        break;
    }
    default:
        break;
    }

    /* We use Qt::QueuedConnection as our handler may start the event loop. */
    connect(item, SIGNAL(actionTriggered(Qn::ActionId, const QnActionParameters &)), this, SLOT(at_item_actionTriggered(Qn::ActionId, const QnActionParameters &)), Qt::QueuedConnection);
    m_list->addItem(item);
}

QnNotificationItem* QnNotificationsCollectionWidget::findItem(QnSystemHealth::MessageType message, const QnResourcePtr &resource) {
    QList<QnNotificationItem*> items = m_itemsByMessageType.values(message);
    foreach (QnNotificationItem* item, items) {
        if (resource != item->property(itemResourcePropertyName).value<QnResourcePtr>())
            continue;
        return item;
    }
    return NULL;
}

void QnNotificationsCollectionWidget::showSystemHealthMessage(QnSystemHealth::MessageType message, const QnResourcePtr &resource) {
    QnNotificationItem *item = findItem(message, resource);
    if (item)
        return;

    item = new QnNotificationItem(m_list);

    switch (message) {
    case QnSystemHealth::EmailIsEmpty:
        item->addActionButton(
            qnSkin->icon("events/email.png"),
            tr("User Settings"),
            Qn::UserSettingsAction,
            QnActionParameters(context()->user()).withArgument(Qn::FocusElementRole, QString(QLatin1String("email")))
        );
        break;
    case QnSystemHealth::NoLicenses:
        item->addActionButton(
            qnSkin->icon("events/license.png"),
            tr("Licenses"),
            Qn::PreferencesLicensesTabAction
        );
        break;
    case QnSystemHealth::SmtpIsNotSet:
        item->addActionButton(
            qnSkin->icon("events/smtp.png"),
            tr("SMTP Settings"),
            Qn::PreferencesServerTabAction
        );
        break;
    case QnSystemHealth::UsersEmailIsEmpty:
        item->addActionButton(
            qnSkin->icon("events/email.png"),
            tr("User Settings"),
            Qn::UserSettingsAction,
            QnActionParameters(resource).withArgument(Qn::FocusElementRole, QString(QLatin1String("email")))
        );
        break;
    case QnSystemHealth::ConnectionLost:
        item->addActionButton(
            qnSkin->icon("events/connection.png"),
            tr("Connect to server"),
            Qn::ConnectToServerAction
        );
        break;
    case QnSystemHealth::EmailSendError:
        item->addActionButton(
            qnSkin->icon("events/email.png"),
            tr("SMTP Settings"),
            Qn::PreferencesServerTabAction
        );
        break;
    case QnSystemHealth::StoragesNotConfigured:
        item->addActionButton(
            qnSkin->icon("events/storage.png"),
            tr("Server settings"),
            Qn::ServerSettingsAction,
            QnActionParameters(resource)
        );
        break;
    case QnSystemHealth::StoragesAreFull:
        item->addActionButton(
            qnSkin->icon("events/storage.png"),
            tr("Server settings"),
            Qn::ServerSettingsAction,
            QnActionParameters(resource)
        );
        break;
    default:
        break;
    }

    QString resourceName = getResourceName(resource);
    item->setText(QnSystemHealthStringsHelper::messageName(message, resourceName));
    item->setTooltipText(QnSystemHealthStringsHelper::messageDescription(message, resourceName));
    item->setNotificationLevel(Qn::SystemNotification);
    item->setProperty(itemResourcePropertyName, QVariant::fromValue<QnResourcePtr>(resource));

    connect(item, SIGNAL(actionTriggered(Qn::ActionId, const QnActionParameters&)), this, SLOT(at_item_actionTriggered(Qn::ActionId, const QnActionParameters&)));

    m_list->addItem(item, message != QnSystemHealth::ConnectionLost);
    m_itemsByMessageType.insert(message, item);
}

void QnNotificationsCollectionWidget::hideSystemHealthMessage(QnSystemHealth::MessageType message, const QnResourcePtr &resource) {
    if (!resource) {
        foreach (QnNotificationItem* item, m_itemsByMessageType.values(message))
            m_list->removeItem(item);
        m_itemsByMessageType.remove(message);
        return;
    }

    QnNotificationItem* target = findItem(message, resource);
    if (!target)
        return;
    m_list->removeItem(target);
    m_itemsByMessageType.remove(message, target);
}

void QnNotificationsCollectionWidget::hideAll() {
    m_list->clear();
    m_itemsByMessageType.clear();
}

void QnNotificationsCollectionWidget::updateBlinker() {
    if(!blinker())
        return;

    blinker()->setNotificationCount(m_list->itemCount());
    blinker()->setColor(QnNotificationLevels::notificationColor(m_list->notificationLevel()));
}

void QnNotificationsCollectionWidget::at_settingsButton_clicked() {
    menu()->trigger(Qn::BusinessEventsAction);
}

void QnNotificationsCollectionWidget::at_filterButton_clicked() {
    menu()->trigger(Qn::PreferencesNotificationTabAction);
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
            if (!sampleServer)
                continue;
            resource = sampleServer;
            break;
        default:
            break;
        }
        showSystemHealthMessage(message, resource);
    }

    //TODO: #GDM REMOVE DEBUG
    for (int i = 0; i < BusinessEventType::Count; i++) {
        BusinessEventType::Value eventType = BusinessEventType::Value(i);

        QnBusinessEventParameters params;
        params.setEventType(eventType);
        params.setEventTimestamp((quint64)QDateTime::currentMSecsSinceEpoch() * 1000ull);
        switch(eventType) {
        case BusinessEventType::Camera_Motion: {
                if (!sampleCamera)
                    continue;
                params.setEventResourceId(sampleCamera->getId());
                break;
            }

        case BusinessEventType::Camera_Input: {
                if (!sampleCamera)
                    continue;
                params.setEventResourceId(sampleCamera->getId());
                params.setInputPortId(lit("01"));
                break;
            }

        case BusinessEventType::Camera_Disconnect: {
                if (!sampleCamera)
                    continue;
                params.setEventResourceId(sampleCamera->getId());
                break;
            }

        case BusinessEventType::Network_Issue: {
                if (!sampleCamera)
                    continue;
                params.setEventResourceId(sampleCamera->getId());
                params.setReasonCode(QnBusiness::NetworkIssueNoFrame);
                params.setReasonText(lit("15"));
                break;
            }

        case BusinessEventType::Storage_Failure: {
                if (!sampleServer)
                    continue;
                params.setEventResourceId(sampleServer->getId());
                params.setReasonCode(QnBusiness::StorageIssueNotEnoughSpeed);
                params.setReasonText(lit("C: E:"));
                break;
            }

        case BusinessEventType::Camera_Ip_Conflict: {
                if (!sampleServer)
                    continue;
                params.setEventResourceId(sampleServer->getId());
                params.setSource(lit("192.168.0.5"));

                QStringList conflicts;
                conflicts << lit("50:e5:49:43:b2:59");
                conflicts << lit("50:e5:49:43:b2:60");
                conflicts << lit("50:e5:49:43:b2:61");
                conflicts << lit("50:e5:49:43:b2:62");
                conflicts << lit("50:e5:49:43:b2:63");
                conflicts << lit("50:e5:49:43:b2:64");
                params.setConflicts(conflicts);
                break;
            }
        case BusinessEventType::MediaServer_Failure: {
                if (!sampleServer)
                    continue;
                params.setEventResourceId(sampleServer->getId());
                params.setReasonCode(QnBusiness::MServerIssueTerminated);
                break;
            }

        case BusinessEventType::MediaServer_Conflict: {
                if (!sampleServer)
                    continue;
                params.setEventResourceId(sampleServer->getId());
                params.setSource(lit("10.0.2.187"));

                QStringList conflicts;
                conflicts << lit("10.0.2.108");
                conflicts << lit("192.168.0.15");
                params.setConflicts(conflicts);
                break;
            }
        default:
            break;

        }

        QnAbstractBusinessActionPtr baction(new QnCommonBusinessAction(BusinessActionType::ShowPopup, params));
        baction->setAggregationCount(random(1, 5));
        showBusinessAction(baction);
    }
}

void QnNotificationsCollectionWidget::at_list_itemRemoved(QnNotificationItem *item) {
    for (int i = 0; i < QnSystemHealth::MessageTypeCount; i++) {
        QnSystemHealth::MessageType message = QnSystemHealth::MessageType(i);
        if (m_itemsByMessageType.remove(message, item) > 0)
            break;
    }
    qnDeleteLater(item);
}

void QnNotificationsCollectionWidget::at_item_actionTriggered(Qn::ActionId actionId, const QnActionParameters &parameters) {
    menu()->trigger(actionId, parameters);
}

