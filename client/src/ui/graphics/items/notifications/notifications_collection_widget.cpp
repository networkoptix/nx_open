#include "notifications_collection_widget.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsLinearLayout>

#include <utils/common/delete_later.h>
#include <utils/common/util.h> /* For random. */

#include <business/business_strings_helper.h>

#include <camera/single_thumbnail_loader.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include <client/client_settings.h>

#include <ui/animation/opacity_animator.h>
#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/common/geometry.h>
#include <ui/common/ui_resource_name.h>
#include <ui/common/notification_levels.h>
#include <ui/graphics/items/generic/particle_item.h>
#include <ui/graphics/items/generic/tool_tip_widget.h>
#include <ui/graphics/items/notifications/notification_widget.h>
#include <ui/graphics/items/notifications/notification_list_widget.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/help/business_help.h>
#include <ui/style/skin.h>
#include <ui/style/globals.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/handlers/workbench_notifications_handler.h>

#include <utils/math/color_transformations.h>
#include <utils/app_server_notification_cache.h>

//TODO: #GDM #Business remove debug
#include <business/actions/common_business_action.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>

namespace {
    const qreal widgetHeight = 24;
    const QSize thumbnailSize(0, 100);

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

    connect(m_balloon,  &QGraphicsWidget::geometryChanged,  this, &QnBlinkingImageButtonWidget::updateBalloonTailPos);
    connect(this,       &QGraphicsWidget::geometryChanged,  this, &QnBlinkingImageButtonWidget::updateParticleGeometry);
    connect(this,       &QnImageButtonWidget::toggled,      this, &QnBlinkingImageButtonWidget::updateParticleVisibility);
    connect(m_particle, &QGraphicsObject::visibleChanged,   this, &QnBlinkingImageButtonWidget::at_particle_visibleChanged);

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

    auto newButton = [this, buttonSize](Qn::ActionId actionId, int helpTopicId) {
        QnImageButtonWidget *button = new QnImageButtonWidget(m_headerWidget);
        button->setDefaultAction(action(actionId));
        button->setFixedSize(buttonSize);
        button->setCached(true);
        if (helpTopicId >= 0)
            setHelpTopic(button, helpTopicId);
        connect(this->context(), &QnWorkbenchContext::userChanged, this, [this, button, actionId] {
            button->setVisible(this->menu()->canTrigger(actionId));
        });
        button->setVisible(this->menu()->canTrigger(actionId));
        return button;
    };

    qreal margin = (widgetHeight - buttonSize) / 2.0;
    QGraphicsLinearLayout *controlsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    controlsLayout->setSpacing(2.0);
    controlsLayout->setContentsMargins(2.0, margin, 2.0, margin);
    controlsLayout->addStretch();

#ifdef _DEBUG
    if(qnSettings->isDevMode()) {
        QnImageButtonWidget *debugButton = new QnImageButtonWidget(m_headerWidget);
        debugButton->setIcon(qnSkin->icon("item/search.png"));
        debugButton->setToolTip(tr("DEBUG"));
        debugButton->setFixedSize(buttonSize);
        debugButton->setCached(true);
        connect(debugButton, &QnImageButtonWidget::clicked, this, &QnNotificationsCollectionWidget::at_debugButton_clicked);
        controlsLayout->addItem(debugButton);
    }
#endif // DEBUG
        
    controlsLayout->addItem(newButton(Qn::BusinessEventsLogAction, Qn::MainWindow_Notifications_EventLog_Help));
    controlsLayout->addItem(newButton(Qn::BusinessEventsAction, -1));
    controlsLayout->addItem(newButton(Qn::PreferencesNotificationTabAction, -1));
    m_headerWidget->setLayout(controlsLayout);

    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(Qt::Vertical);
    layout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    layout->setSpacing(0.0);
    layout->addItem(m_headerWidget);

    m_list = new QnNotificationListWidget(this);
    layout->addItem(m_list);

    connect(m_list, &QnNotificationListWidget::itemRemoved,         this, &QnNotificationsCollectionWidget::at_list_itemRemoved);
    connect(m_list, &QnNotificationListWidget::visibleSizeChanged,  this, &QnNotificationsCollectionWidget::visibleSizeChanged);
    connect(m_list, &QnNotificationListWidget::sizeHintChanged,     this, &QnNotificationsCollectionWidget::sizeHintChanged);

    setLayout(layout);

    QnWorkbenchNotificationsHandler *handler = this->context()->instance<QnWorkbenchNotificationsHandler>();
    connect(handler,    &QnWorkbenchNotificationsHandler::businessActionAdded,      this,   &QnNotificationsCollectionWidget::showBusinessAction);
    connect(handler,    &QnWorkbenchNotificationsHandler::businessActionRemoved,    this,   &QnNotificationsCollectionWidget::hideBusinessAction);
    connect(handler,    &QnWorkbenchNotificationsHandler::systemHealthEventAdded,   this,   &QnNotificationsCollectionWidget::showSystemHealthMessage);
    connect(handler,    &QnWorkbenchNotificationsHandler::systemHealthEventRemoved, this,   &QnNotificationsCollectionWidget::hideSystemHealthMessage);
    connect(handler,    &QnWorkbenchNotificationsHandler::cleared,                  this,   &QnNotificationsCollectionWidget::hideAll);
    connect(this->context()->instance<QnAppServerNotificationCache>(), &QnAppServerNotificationCache::fileDownloaded, this, &QnNotificationsCollectionWidget::at_notificationCache_fileDownloaded);
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
        disconnect(m_list, NULL, m_blinker, NULL);
    
    m_blinker = blinker;
    
    if (m_blinker) {
        connect(m_list, &QnNotificationListWidget::itemCountChanged,         this, &QnNotificationsCollectionWidget::updateBlinker);
        connect(m_list, &QnNotificationListWidget::notificationLevelChanged, this, &QnNotificationsCollectionWidget::updateBlinker);
        updateBlinker();
    }
}

void QnNotificationsCollectionWidget::loadThumbnailForItem(QnNotificationWidget *item, QnResourcePtr resource, qint64 usecsSinceEpoch) {
    QnSingleThumbnailLoader *loader = QnSingleThumbnailLoader::newInstance(resource, usecsSinceEpoch, -1, thumbnailSize, QnSingleThumbnailLoader::JpgFormat, item);
    item->setImageProvider(loader);
}

void QnNotificationsCollectionWidget::showBusinessAction(const QnAbstractBusinessActionPtr &businessAction) {
    QnBusinessEventParameters params = businessAction->getRuntimeParams();
    QnUuid resourceId = params.getEventResourceId();
    QnResourcePtr resource = qnResPool->getResourceById(resourceId);
    if (!resource)
        return;

    if(m_list->itemCount() >= maxNotificationItems)
        return; /* Just drop the notification if we already have too many of them in queue. */

    QnNotificationWidget *item = new QnNotificationWidget(m_list);

    QnBusiness::EventType eventType = params.getEventType();

    item->setText(QnBusinessStringsHelper::eventAtResource(params, qnSettings->isIpShownInTree()));
    item->setTooltipText(QnBusinessStringsHelper::eventDescription(businessAction, QnBusinessAggregationInfo(), qnSettings->isIpShownInTree(), false));

    const bool soundAction = businessAction->actionType() == QnBusiness::PlaySoundAction;
    if (soundAction) {
        QString soundUrl = businessAction->getParams().soundUrl;
        m_itemsByLoadingSound.insert(soundUrl, item);
        context()->instance<QnAppServerNotificationCache>()->downloadFile(soundUrl);
        item->setNotificationLevel(Qn::CommonNotification);
    } else {
        item->setNotificationLevel(QnNotificationLevels::notificationLevel(eventType));
    }

    setHelpTopic(item, QnBusiness::eventHelpId(eventType));

    switch (eventType) {
    case QnBusiness::CameraMotionEvent: {
        QIcon icon = soundAction ? qnSkin->icon("events/sound.png") : qnSkin->icon("events/camera.png");
        item->addActionButton(
            icon,
            tr("Browse Archive"),
            Qn::OpenInNewLayoutAction,
            QnActionParameters(resource).withArgument(Qn::ItemTimeRole, params.getEventTimestamp()/1000)
        );
        loadThumbnailForItem(item, resource, params.getEventTimestamp());
        break;
    }
    case QnBusiness::CameraInputEvent: {
        QIcon icon = soundAction ? qnSkin->icon("events/sound.png") : qnSkin->icon("events/camera.png");
        item->addActionButton(
            icon,
            tr("Open Camera"),
            Qn::OpenInNewLayoutAction,
            QnActionParameters(resource)
        );
        loadThumbnailForItem(item, resource);
        break;
    }
    case QnBusiness::CameraDisconnectEvent: {
        item->addActionButton(
            qnSkin->icon("events/camera.png"),
            tr("Camera Settings"),
            Qn::CameraSettingsAction,
            QnActionParameters(resource)
        );
        loadThumbnailForItem(item, resource);
        break;
    }
    case QnBusiness::StorageFailureEvent: {
        item->addActionButton(
            qnSkin->icon("events/storage.png"),
            tr("Server settings"),
            Qn::ServerSettingsAction,
            QnActionParameters(resource)
        );
        break;
    }
    case QnBusiness::NetworkIssueEvent:{
        item->addActionButton(
            qnSkin->icon("events/server.png"),
            tr("Camera Settings"),
            Qn::CameraSettingsAction,
            QnActionParameters(resource)
        );
        loadThumbnailForItem(item, resource);
        break;
    }
    case QnBusiness::CameraIpConflictEvent: {
        QString webPageAddress = params.getSource();

        item->addActionButton(
            qnSkin->icon("events/camera.png"),
            tr("Open camera web page..."),
            Qn::BrowseUrlAction,
            QnActionParameters().withArgument(Qn::UrlRole, webPageAddress)
        );
        break;
    }
    case QnBusiness::ServerFailureEvent: {
        item->addActionButton(
            qnSkin->icon("events/server.png"),
            tr("Settings"),
            Qn::ServerSettingsAction,
            QnActionParameters(resource)
        );
        break;
    }
    case QnBusiness::ServerConflictEvent:
    case QnBusiness::ServerStartEvent: {
        item->addActionButton(
            qnSkin->icon("events/server.png"),
            QString(),
            Qn::NoAction
        );
        break;
    }
    case QnBusiness::LicenseIssueEvent: {
        item->addActionButton(
            qnSkin->icon("events/license.png"),
            QString(),
            Qn::PreferencesLicensesTabAction
            );
        break;
                                       }

    default:
        break;
    }


    item->setProperty(itemResourcePropertyName, QVariant::fromValue<QnResourcePtr>(resource));
    m_itemsByBusinessRuleId.insert(businessAction->getBusinessRuleId(), item);

    /* We use Qt::QueuedConnection as our handler may start the event loop. */
    connect(item, &QnNotificationWidget::actionTriggered, this, &QnNotificationsCollectionWidget::at_item_actionTriggered, Qt::QueuedConnection);

    m_list->addItem(item, businessAction->actionType() == QnBusiness::PlaySoundAction);
}

void QnNotificationsCollectionWidget::hideBusinessAction(const QnAbstractBusinessActionPtr &businessAction) {
    QnUuid ruleId = businessAction->getBusinessRuleId();
    QnResourcePtr resource = qnResPool->getResourceById(businessAction->getRuntimeParams().getEventResourceId());
    if (!resource)
        return;

    // TODO: #GDM #Business please review, there must be a better way to do this. 
    // Probably PlaySoundRepeated is not the only action type. See #2812.
    QnNotificationWidget* item = findItem(ruleId, resource, businessAction->actionType() != QnBusiness::PlaySoundAction); /* Ignore resource for repeated sound actions. */
    if (!item)
        return;
    m_list->removeItem(item);
    m_itemsByBusinessRuleId.remove(ruleId, item);
}

QnNotificationWidget* QnNotificationsCollectionWidget::findItem(QnSystemHealth::MessageType message, const QnResourcePtr &resource, bool useResource) {
    foreach (QnNotificationWidget *item, m_itemsByMessageType.values(message))
        if (!useResource || resource == item->property(itemResourcePropertyName).value<QnResourcePtr>())
            return item;
    return NULL;
}

QnNotificationWidget* QnNotificationsCollectionWidget::findItem(const QnUuid& businessRuleId, const QnResourcePtr &resource, bool useResource) {
    foreach (QnNotificationWidget *item, m_itemsByBusinessRuleId.values(businessRuleId))
        if (!useResource || resource == item->property(itemResourcePropertyName).value<QnResourcePtr>())
            return item;
    return NULL;
}

void QnNotificationsCollectionWidget::showSystemHealthMessage( QnSystemHealth::MessageType message, const QVariant& params )
{
    QnResourcePtr resource;
    if( params.canConvert<QnResourcePtr>() )
        resource = params.value<QnResourcePtr>();

    QnNotificationWidget *item = findItem( message, resource );
    if (item)
        return;

    item = new QnNotificationWidget(m_list);

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
            tr("SMTP Settin gs"),
            Qn::PreferencesSmtpTabAction
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
            Qn::OpenLoginDialogAction
        );
        break;
    case QnSystemHealth::NoPrimaryTimeServer:
    {
        QnActionParameters actionParams;
        if( params.canConvert<QnActionParameters>() )
            actionParams = params.value<QnActionParameters>();
        item->addActionButton(
            qnSkin->icon( "events/settings.png" ),
            QnSystemHealthStringsHelper::messageTitle( QnSystemHealth::NoPrimaryTimeServer ),
            //tr( "Connect to server" ),
            Qn::SelectTimeServerAction,
            actionParams
        );
        break;
    }
    case QnSystemHealth::EmailSendError:
        item->addActionButton(
            qnSkin->icon("events/email.png"),
            tr("SMTP Settings"),
            Qn::PreferencesSmtpTabAction
        );
        break;
    case QnSystemHealth::StoragesNotConfigured:
    case QnSystemHealth::StoragesAreFull:
    case QnSystemHealth::ArchiveRebuildFinished:
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
    item->setNotificationLevel(QnNotificationLevels::notificationLevel(message));
    item->setProperty(itemResourcePropertyName, QVariant::fromValue<QnResourcePtr>(resource));
    setHelpTopic(item, QnBusiness::healthHelpId(message));

    /* We use Qt::QueuedConnection as our handler may start the event loop. */
    connect(item, &QnNotificationWidget::actionTriggered, this, &QnNotificationsCollectionWidget::at_item_actionTriggered, Qt::QueuedConnection);

    m_list->addItem(item, message != QnSystemHealth::ConnectionLost);
    m_itemsByMessageType.insert(message, item);
}

void QnNotificationsCollectionWidget::hideSystemHealthMessage(QnSystemHealth::MessageType message, const QVariant& params) {
    QnResourcePtr resource;
    if( params.canConvert<QnResourcePtr>() )
        resource = params.value<QnResourcePtr>();

    if (!resource) {
        foreach (QnNotificationWidget* item, m_itemsByMessageType.values(message))
            m_list->removeItem(item);
        m_itemsByMessageType.remove(message);
        return;
    }

    QnNotificationWidget* target = findItem(message, resource);
    if (!target)
        return;
    m_list->removeItem(target);
    m_itemsByMessageType.remove(message, target);
}

void QnNotificationsCollectionWidget::hideAll() {
    m_list->clear();
    m_itemsByMessageType.clear();
    m_itemsByLoadingSound.clear();
    m_itemsByBusinessRuleId.clear();
}

void QnNotificationsCollectionWidget::updateBlinker() {
    if(!blinker())
        return;

    blinker()->setNotificationCount(m_list->itemCount());
    blinker()->setColor(QnNotificationLevels::notificationColor(m_list->notificationLevel()));
}

void QnNotificationsCollectionWidget::at_debugButton_clicked() {
    QnResourceList servers = qnResPool->getResources<QnMediaServerResource>();
    QnResourcePtr sampleServer = servers.isEmpty() ? QnResourcePtr() : servers.first();

    QnResourceList cameras = qnResPool->getResources<QnVirtualCameraResource>();
    QnResourcePtr sampleCamera = cameras.isEmpty() ? QnResourcePtr() : cameras.first();

    //TODO: #GDM #Business REMOVE DEBUG
    for (int i = 0; i < QnSystemHealth::MessageTypeCount; i++) {
        QnSystemHealth::MessageType message = QnSystemHealth::MessageType(i);
        QnResourcePtr resource;
        switch (message) {
        case QnSystemHealth::EmailIsEmpty:
            resource = context()->user();
            break;
        case QnSystemHealth::UsersEmailIsEmpty:
            resource = qnResPool->getResources<QnUserResource>().last();
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
        showSystemHealthMessage(message, QVariant::fromValue(resource));
    }

    //TODO: #GDM #Business REMOVE DEBUG
    for (QnBusiness::EventType eventType: QnBusiness::allEvents()) {

        QnBusinessEventParameters params;
        params.setEventType(eventType);
        params.setEventTimestamp((quint64)QDateTime::currentMSecsSinceEpoch() * 1000ull);
        switch(eventType) {
        case QnBusiness::CameraMotionEvent: {
                if (!sampleCamera)
                    continue;
                params.setEventResourceId(sampleCamera->getId());
                break;
            }

        case QnBusiness::CameraInputEvent: {
                if (!sampleCamera)
                    continue;
                params.setEventResourceId(sampleCamera->getId());
                params.setInputPortId(lit("01"));
                break;
            }

        case QnBusiness::CameraDisconnectEvent: {
                if (!sampleCamera)
                    continue;
                params.setEventResourceId(sampleCamera->getId());
                break;
            }

        case QnBusiness::NetworkIssueEvent: {
                if (!sampleCamera)
                    continue;
                params.setEventResourceId(sampleCamera->getId());
                params.setReasonCode(QnBusiness::NetworkNoFrameReason);
                params.setReasonParamsEncoded(lit("15000"));
                break;
            }

        case QnBusiness::StorageFailureEvent: {
                if (!sampleServer)
                    continue;
                params.setEventResourceId(sampleServer->getId());
                params.setReasonCode(QnBusiness::StorageTooSlowReason);
                params.setReasonParamsEncoded(lit("C: E:"));
                break;
            }

        case QnBusiness::CameraIpConflictEvent: {
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
        case QnBusiness::ServerFailureEvent: {
                if (!sampleServer)
                    continue;
                params.setEventResourceId(sampleServer->getId());
                params.setReasonCode(QnBusiness::ServerTerminatedReason);
                break;
            }

        case QnBusiness::ServerConflictEvent: {
                if (!sampleServer)
                    continue;
                params.setEventResourceId(sampleServer->getId());
                params.setSource(lit("10.0.2.187"));

                QStringList conflicts;
                conflicts << lit("10.0.2.108");
                conflicts << lit("2");
                conflicts << lit("50:e5:49:43:b2:63");
                conflicts << lit("50:e5:49:43:b2:64");
                conflicts << lit("192.168.0.15");
                conflicts << lit("4");
                conflicts << lit("50:e5:49:43:b2:61");
                conflicts << lit("50:e5:49:43:b2:62");
                conflicts << lit("50:e5:49:43:b2:63");
                conflicts << lit("50:e5:49:43:b2:64");
                params.setConflicts(conflicts);
                break;
            }
        default:
            break;

        }

        QnAbstractBusinessActionPtr baction(new QnCommonBusinessAction(QnBusiness::ShowPopupAction, params));
        baction->setAggregationCount(random(1, 5));
        showBusinessAction(baction);
    }
}

void QnNotificationsCollectionWidget::at_list_itemRemoved(QnNotificationWidget *item) {
    foreach (QnSystemHealth::MessageType messageType, m_itemsByMessageType.keys(item))
        m_itemsByMessageType.remove(messageType, item);

    foreach (const QnUuid& ruleId, m_itemsByBusinessRuleId.keys(item))
        m_itemsByBusinessRuleId.remove(ruleId, item);

    foreach (QString soundPath, m_itemsByLoadingSound.keys(item))
        m_itemsByLoadingSound.remove(soundPath, item);

    qnDeleteLater(item);
}

void QnNotificationsCollectionWidget::at_item_actionTriggered(Qn::ActionId actionId, const QnActionParameters &parameters) {
    menu()->trigger(actionId, parameters);
}

void QnNotificationsCollectionWidget::at_notificationCache_fileDownloaded(const QString &filename, bool ok) {
    if (!ok)
        return;

    QString filePath = context()->instance<QnAppServerNotificationCache>()->getFullPath(filename);
    foreach (QnNotificationWidget* item, m_itemsByLoadingSound.values(filename)) {
        item->setSound(filePath, true);
    }
    m_itemsByLoadingSound.remove(filename);
}
