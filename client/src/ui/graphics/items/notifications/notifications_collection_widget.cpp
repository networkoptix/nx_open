#include "notifications_collection_widget.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsLinearLayout>

#include <nx/utils/log/log.h>

#include <business/business_strings_helper.h>

#include <camera/single_thumbnail_loader.h>
#include <camera/camera_thumbnail_manager.h>

#include <core/resource/resource.h>
#include <core/resource/resource_name.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

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
#include <health/system_health_helper.h>

#include <utils/common/delete_later.h>
#include <utils/common/util.h> /* For random. */
#include <utils/math/color_transformations.h>
#include <utils/app_server_notification_cache.h>
#include <utils/multi_image_provider.h>

#include <utils/common/model_functions.h>

namespace
{
    const QSize kDefaultThumbnailSize(0, QnThumbnailRequestData::kMinimumSize);

    /** We limit the maximal number of notification items to prevent crashes due
     * to reaching GDI resource limit. */
    const int maxNotificationItems = 128;
    const int multiThumbnailSpacing = 4;


    const char *itemResourcePropertyName    = "_qn_itemResource";
    const char *itemActionTypePropertyName  = "_qn_itemActionType";
    const char *itemTimeStampPropertyName   = "_qn_itemTimeStamp";

} //anonymous namespace

QnBlinkingImageButtonWidget::QnBlinkingImageButtonWidget(const QString &statisticsAlias
    , QGraphicsItem *parent):
    base_type(statisticsAlias, parent),
    m_time(0),
    m_count(0)
{
    registerAnimation(this);
    startListening();

    m_particle = new QnParticleItem(this);

    m_balloon = new QnToolTipWidget(this);
    m_balloon->setText(tr("You have new notifications."));
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


QnNotificationsCollectionWidget::QnNotificationsCollectionWidget(QGraphicsItem *parent, Qt::WindowFlags flags, QnWorkbenchContext* context)
    : base_type(parent, flags)
    , QnWorkbenchContextAware(context)
    , m_headerWidget(new GraphicsWidget(this))
    , m_statusPixmapManager(new QnCameraThumbnailManager())
{
    m_statusPixmapManager->setThumbnailSize(kDefaultThumbnailSize);

    int maxIconSize = QApplication::style()->pixelMetric(QStyle::PM_ToolBarIconSize, NULL, NULL);
    auto newButton = [this, maxIconSize](QnActions::IDType actionId, int helpTopicId)
    {
        const auto statAlias = lit("%1_%2").arg(lit("notifications_collection_widget")
            , QnLexical::serialized(actionId));
        QnImageButtonWidget *button = new QnImageButtonWidget(statAlias, m_headerWidget);
        button->setDefaultAction(action(actionId));
        button->setFixedSize(button->defaultAction()->icon().actualSize(QSize(maxIconSize, maxIconSize)));
        button->setCached(true);
        if (helpTopicId != Qn::Empty_Help)
            setHelpTopic(button, helpTopicId);
        connect(this->context(), &QnWorkbenchContext::userChanged, this, [this, button, actionId] {
            button->setVisible(this->menu()->canTrigger(actionId));
        });
        button->setVisible(this->menu()->canTrigger(actionId));
        return button;
    };

    QGraphicsLinearLayout *controlsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    controlsLayout->setSpacing(2.0);
    controlsLayout->setContentsMargins(2.0, 1.0, 2.0, 0.0);
    controlsLayout->addStretch();

    controlsLayout->addItem(newButton(QnActions::OpenBusinessLogAction, Qn::MainWindow_Notifications_EventLog_Help));
    controlsLayout->addItem(newButton(QnActions::BusinessEventsAction, -1));
    controlsLayout->addItem(newButton(QnActions::PreferencesNotificationTabAction, -1));
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
    connect(handler,    &QnWorkbenchNotificationsHandler::notificationAdded,        this,   &QnNotificationsCollectionWidget::showBusinessAction);
    connect(handler,    &QnWorkbenchNotificationsHandler::notificationRemoved,      this,   &QnNotificationsCollectionWidget::hideBusinessAction);
    connect(handler,    &QnWorkbenchNotificationsHandler::systemHealthEventAdded,   this,   &QnNotificationsCollectionWidget::showSystemHealthMessage);
    connect(handler,    &QnWorkbenchNotificationsHandler::systemHealthEventRemoved, this,   &QnNotificationsCollectionWidget::hideSystemHealthMessage);
    connect(handler,    &QnWorkbenchNotificationsHandler::cleared,                  this,   &QnNotificationsCollectionWidget::hideAll);
    connect(this->context()->instance<QnAppServerNotificationCache>(), &QnAppServerNotificationCache::fileDownloaded,
        this, [this](const QString &filename, QnAppServerFileCache::OperationResult status) {
            if (status != QnAppServerFileCache::OperationResult::ok)
                return;
            at_notificationCache_fileDownloaded(filename);
    });
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

void QnNotificationsCollectionWidget::loadThumbnailForItem(QnNotificationWidget *item,
                                                           const QnVirtualCameraResourcePtr &camera,
                                                           qint64 msecSinceEpoch)
{
    if (!camera || !camera->hasVideo(nullptr))
        return;

    QnSingleThumbnailLoader *loader = new QnSingleThumbnailLoader(
          camera
        , msecSinceEpoch
        , QnThumbnailRequestData::kDefaultRotation
        , kDefaultThumbnailSize
        , QnThumbnailRequestData::JpgFormat
        , m_statusPixmapManager
        , item
        );
    item->setImageProvider(loader);
}

void QnNotificationsCollectionWidget::loadThumbnailForItem(QnNotificationWidget *item,
                                                           const QnVirtualCameraResourceList &cameraList,
                                                           qint64 msecSinceEpoch)
{
    if (cameraList.isEmpty())
        return;

    QnMultiImageProvider::Providers providers;
    for (const auto& camera: cameraList) {
        std::unique_ptr<QnImageProvider> provider(new QnSingleThumbnailLoader(
              camera
            , msecSinceEpoch
            , QnThumbnailRequestData::kDefaultRotation
            , kDefaultThumbnailSize
            , QnThumbnailRequestData::JpgFormat
            , m_statusPixmapManager
            ));
        providers.push_back(std::move(provider));
    }
    item->setImageProvider(new QnMultiImageProvider(std::move(providers), Qt::Vertical, multiThumbnailSpacing, item));
}

void QnNotificationsCollectionWidget::showBusinessAction(const QnAbstractBusinessActionPtr &businessAction) {
    if(m_list->itemCount() >= maxNotificationItems)
        return; /* Just drop the notification if we already have too many of them in queue. */

    QnBusinessEventParameters params = businessAction->getRuntimeParams();
    QnBusiness::EventType eventType = params.eventType;
    QnUuid ruleId = businessAction->getBusinessRuleId();
    QString title = QnBusinessStringsHelper::eventAtResource(params, qnSettings->extraInfoInTree());
    qint64 timestampMs = params.eventTimestampUsec / 1000;

    QnVirtualCameraResourceList alarmCameras = qnResPool->getResources<QnVirtualCameraResource>(businessAction->getResources());
    if (businessAction->getParams().useSource)
        alarmCameras << qnResPool->getResources<QnVirtualCameraResource>(businessAction->getSourceResources());

    QnResourcePtr resource = qnResPool->getResourceById(params.eventResourceId);

    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera && QnBusiness::isSourceCameraRequired(eventType))
    {
        NX_ASSERT(false, Q_FUNC_INFO, "Event has occurred without its camera");
        NX_LOG(lit("Event %1 has occurred without its camera").arg(eventType), cl_logWARNING);
        return;
    }

    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server && QnBusiness::isSourceServerRequired(eventType))
    {
        NX_ASSERT(false, Q_FUNC_INFO, "Event has occurred without its server");
        NX_LOG(lit("Event %1 has occurred without its server").arg(eventType), cl_logWARNING);
        return;
    }

    if (businessAction->actionType() == QnBusiness::ShowOnAlarmLayoutAction) {
        if (alarmCameras.isEmpty())
            return;

        if (findItem(ruleId, [timestampMs](QnNotificationWidget* item) {
            return item->property(itemActionTypePropertyName) == QnBusiness::ShowOnAlarmLayoutAction
                && item->property(itemTimeStampPropertyName)  == timestampMs;
        }))
            return; /* Show 'Alarm Layout' notifications only once for each event of one rule. */

        title = tr("Alarm: %1").arg(title);
    }

    QnNotificationWidget *item = new QnNotificationWidget(m_list);
    item->setText(title);
    item->setTooltipText(QnBusinessStringsHelper::eventDescription(businessAction, QnBusinessAggregationInfo(), qnSettings->extraInfoInTree(), false));
    item->setNotificationLevel(QnNotificationLevel::valueOf(businessAction));
    item->setProperty(itemResourcePropertyName,   QVariant::fromValue<QnResourcePtr>(resource));
    item->setProperty(itemActionTypePropertyName, businessAction->actionType());
    item->setProperty(itemTimeStampPropertyName,  timestampMs);
    setHelpTopic(item, QnBusiness::eventHelpId(eventType));

    if (businessAction->actionType() == QnBusiness::PlaySoundAction) {
        QString soundUrl = businessAction->getParams().url;
        m_itemsByLoadingSound.insert(soundUrl, item);
        context()->instance<QnAppServerNotificationCache>()->downloadFile(soundUrl);
    }

    QIcon icon = iconForAction(businessAction);

    enum { kMaxThumbnailCount = 5 };
    if (businessAction->actionType() == QnBusiness::ShowOnAlarmLayoutAction) {
        item->addActionButton(
            icon,
            tr("Open in Alarm Layout"),
            QnActions::OpenInAlarmLayoutAction,
            QnActionParameters(alarmCameras)
            );
        loadThumbnailForItem(item, alarmCameras.mid(0, kMaxThumbnailCount));
    }
    else
    {
        switch (eventType)
        {
        case QnBusiness::CameraMotionEvent:
        {
            item->addActionButton(
                icon,
                tr("Browse Archive"),
                QnActions::OpenInNewLayoutAction,
                QnActionParameters(camera).withArgument(Qn::ItemTimeRole, timestampMs)
                );
            loadThumbnailForItem(item, camera, timestampMs);
            break;
        }
        case QnBusiness::CameraInputEvent:
        {
            item->addActionButton(
                icon,
                QnDeviceDependentStrings::getNameFromSet(
                    QnCameraDeviceStringSet(
                        tr("Open Device"),
                        tr("Open Camera"),
                        tr("Open I/O Module")
                    ), camera
                ),
                QnActions::OpenInNewLayoutAction,
                QnActionParameters(camera)
            );
            loadThumbnailForItem(item, camera);
            break;
        }

        case QnBusiness::CameraDisconnectEvent:
        case QnBusiness::NetworkIssueEvent:
        {
            item->addActionButton(
                icon,
                QnDeviceDependentStrings::getNameFromSet(
                    QnCameraDeviceStringSet(
                        tr("Device Settings..."),
                        tr("Camera Settings..."),
                        tr("I/O Module Settings...")
                    ), camera
                ),
                QnActions::CameraSettingsAction,
                QnActionParameters(camera)
            );
            loadThumbnailForItem(item, camera);
            break;
        }

        case QnBusiness::StorageFailureEvent:
        case QnBusiness::BackupFinishedEvent:
        case QnBusiness::ServerStartEvent:
        case QnBusiness::ServerFailureEvent:
        {
            item->addActionButton(
                icon,
                tr("Server Settings..."),
                QnActions::ServerSettingsAction,
                QnActionParameters(server)
            );
            break;
        }

        case QnBusiness::CameraIpConflictEvent:
        {
            QString webPageAddress = params.caption;

            item->addActionButton(
                icon,
                QnDeviceDependentStrings::getNameFromSet(
                    QnCameraDeviceStringSet(
                        tr("Open Device Web Page..."),
                        tr("Open Camera Web Page..."),
                        tr("Open I/O Module Web Page...")
                    ), camera
                ),
                QnActions::BrowseUrlAction,
                QnActionParameters().withArgument(Qn::UrlRole, webPageAddress)
            );
            break;
        }

        case QnBusiness::ServerConflictEvent:
        {
            item->addActionButton(icon);
            break;
        }

        case QnBusiness::LicenseIssueEvent:
        {
            item->addActionButton(
                icon,
                tr("Licenses..."),
                QnActions::PreferencesLicensesTabAction
                );
            break;
        }

        case QnBusiness::UserDefinedEvent:
        {
            QnVirtualCameraResourceList sourceCameras = qnResPool->getResources<QnVirtualCameraResource>(params.metadata.cameraRefs);
            if (!sourceCameras.isEmpty()) {
                item->addActionButton(
                    icon,
                    tr("Browse Archive"),
                    QnActions::OpenInNewLayoutAction,
                    QnActionParameters(sourceCameras).withArgument(Qn::ItemTimeRole, timestampMs)
                    );
                loadThumbnailForItem(item, sourceCameras.mid(0, kMaxThumbnailCount), timestampMs);
            }
            break;
        }

        default:
            break;
        }
    }


    m_itemsByBusinessRuleId.insert(ruleId, item);

    /* We use Qt::QueuedConnection as our handler may start the event loop. */
    connect(item, &QnNotificationWidget::actionTriggered, this, &QnNotificationsCollectionWidget::at_item_actionTriggered, Qt::QueuedConnection);

    m_list->addItem(item, businessAction->actionType() == QnBusiness::PlaySoundAction);
}

void QnNotificationsCollectionWidget::hideBusinessAction(const QnAbstractBusinessActionPtr &businessAction) {
    QnUuid ruleId = businessAction->getBusinessRuleId();

    if (businessAction->actionType() == QnBusiness::PlaySoundAction) {
        for (QnNotificationWidget *item: m_itemsByBusinessRuleId.values(ruleId)) {
            m_list->removeItem(item);
            cleanUpItem(item);
        }
    }

    QnResourcePtr resource = qnResPool->getResourceById(businessAction->getRuntimeParams().eventResourceId);
    if (!resource)
        return;

    QnNotificationWidget* item = findItem(ruleId, resource);
    if (!item)
        return;
    m_list->removeItem(item);
    cleanUpItem(item);
}

QnNotificationWidget* QnNotificationsCollectionWidget::findItem(QnSystemHealth::MessageType message, const QnResourcePtr &resource) {
    for (QnNotificationWidget *item: m_itemsByMessageType.values(message))
        if (resource == item->property(itemResourcePropertyName).value<QnResourcePtr>())
            return item;
    return nullptr;
}

QnNotificationWidget* QnNotificationsCollectionWidget::findItem(const QnUuid& businessRuleId, const QnResourcePtr &resource) {
    for (QnNotificationWidget *item: m_itemsByBusinessRuleId.values(businessRuleId))
        if (resource == item->property(itemResourcePropertyName).value<QnResourcePtr>())
            return item;
    return nullptr;
}

QnNotificationWidget* QnNotificationsCollectionWidget::findItem(const QnUuid& businessRuleId, std::function<bool(QnNotificationWidget* item)> filter ) {
    for (QnNotificationWidget* item: m_itemsByBusinessRuleId.values(businessRuleId)) {
        if (!filter(item))
            continue;
        return item;
    }
    return nullptr;
}

QIcon QnNotificationsCollectionWidget::iconForAction( const QnAbstractBusinessActionPtr& businessAction ) const {
    if (businessAction->actionType() == QnBusiness::PlaySoundAction)
        return qnSkin->icon("events/sound.png");

    if (businessAction->actionType() == QnBusiness::ShowOnAlarmLayoutAction)
        return qnSkin->icon("events/alarm.png");

    auto params = businessAction->getRuntimeParams();
    QnBusiness::EventType eventType = params.eventType;

    if (eventType >= QnBusiness::UserDefinedEvent) {
        QnVirtualCameraResourceList camList = qnResPool->getResources<QnVirtualCameraResource>(params.metadata.cameraRefs);
        if (!camList.isEmpty())
            return qnSkin->icon("events/camera.png");
        return QIcon();
    }

    switch (eventType) {
    case QnBusiness::CameraMotionEvent:
    case QnBusiness::CameraInputEvent:
    case QnBusiness::CameraDisconnectEvent:
    case QnBusiness::CameraIpConflictEvent:
    case QnBusiness::NetworkIssueEvent:
        //TODO: #GDM #design #3.0 change icon if the device was I/O Module
        return qnSkin->icon("events/camera.png");

    case QnBusiness::StorageFailureEvent:
        return qnSkin->icon("events/storage.png");

    case QnBusiness::ServerStartEvent:
    case QnBusiness::ServerFailureEvent:
    case QnBusiness::ServerConflictEvent:
    case QnBusiness::BackupFinishedEvent:
        return qnSkin->icon("events/server.png");

    case QnBusiness::LicenseIssueEvent:
        return qnSkin->icon("events/license.png");
    default:
        break;
    }

    return QIcon();
}


void QnNotificationsCollectionWidget::showSystemHealthMessage( QnSystemHealth::MessageType message, const QVariant& params ) {
    QString messageText = QnSystemHealthStringsHelper::messageName(message);
    NX_ASSERT(!messageText.isEmpty(), Q_FUNC_INFO, "Undefined system health message ");
    if (messageText.isEmpty())
        return;

    QnResourcePtr resource;
    if( params.canConvert<QnResourcePtr>() )
        resource = params.value<QnResourcePtr>();

    QnAbstractBusinessActionPtr businessAction;
    if( params.canConvert<QnAbstractBusinessActionPtr>() )
    {
        businessAction = params.value<QnAbstractBusinessActionPtr>();
        if (businessAction) {
            auto resourceId = businessAction->getRuntimeParams().eventResourceId;
            resource = qnResPool->getResourceById(resourceId);
        }
    }

    QnNotificationWidget *item = findItem( message, resource );
    if (item)
        return;

    item = new QnNotificationWidget(m_list);

    QnActionParameters actionParams;
    if( params.canConvert<QnActionParameters>() )
        actionParams = params.value<QnActionParameters>();

    switch (message) {
    case QnSystemHealth::EmailIsEmpty:
        item->addActionButton(
            qnSkin->icon("events/email.png"),
            tr("User Settings..."),
            QnActions::UserSettingsAction,
            QnActionParameters(context()->user()).withArgument(Qn::FocusElementRole, QString(QLatin1String("email")))
        );
        break;
    case QnSystemHealth::NoLicenses:
        item->addActionButton(
            qnSkin->icon("events/license.png"),
            tr("Licenses..."),
            QnActions::PreferencesLicensesTabAction
        );
        break;
    case QnSystemHealth::SmtpIsNotSet:
        item->addActionButton(
            qnSkin->icon("events/smtp.png"),
            tr("SMTP Settings..."),
            QnActions::PreferencesSmtpTabAction
        );
        break;
    case QnSystemHealth::UsersEmailIsEmpty:
        item->addActionButton(
            qnSkin->icon("events/email.png"),
            tr("User Settings..."),
            QnActions::UserSettingsAction,
            QnActionParameters(resource).withArgument(Qn::FocusElementRole, QString(QLatin1String("email")))
        );
        break;
    case QnSystemHealth::ConnectionLost:
        item->addActionButton(
            qnSkin->icon("events/connection.png"),
            tr("Connect to server..."),
            QnActions::OpenLoginDialogAction
        );
        break;
    case QnSystemHealth::NoPrimaryTimeServer:
    {
        item->addActionButton(
            qnSkin->icon( "events/settings.png" ),
            tr("Time Synchronization..."),
            QnActions::SelectTimeServerAction,
            actionParams
        );
        break;
    }
    case QnSystemHealth::SystemIsReadOnly:
    {
        break;
    }
    case QnSystemHealth::EmailSendError:
        item->addActionButton(
            qnSkin->icon("events/email.png"),
            tr("SMTP Settings..."),
            QnActions::PreferencesSmtpTabAction
        );
        break;
    case QnSystemHealth::StoragesNotConfigured:
    case QnSystemHealth::StoragesAreFull:
    case QnSystemHealth::ArchiveRebuildFinished:
    case QnSystemHealth::ArchiveRebuildCanceled:
        item->addActionButton(
            qnSkin->icon("events/storage.png"),
            tr("Server settings..."),
            QnActions::ServerSettingsAction,
            QnActionParameters(resource)
        );
        break;
    default:
        NX_ASSERT(false, Q_FUNC_INFO, "Undefined system health message ");
        break;
    }

    QString resourceName = getResourceName(resource);
    item->setText(QnSystemHealthStringsHelper::messageName(message, resourceName));
    item->setTooltipText(QnSystemHealthStringsHelper::messageDescription(message, resourceName));
    item->setNotificationLevel(QnNotificationLevel::valueOf(message));
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
    blinker()->setColor(QnNotificationLevel::notificationColor(m_list->notificationLevel()));
}

void QnNotificationsCollectionWidget::at_list_itemRemoved(QnNotificationWidget *item) {
    cleanUpItem(item);
    qnDeleteLater(item);
}

void QnNotificationsCollectionWidget::at_item_actionTriggered(QnActions::IDType actionId, const QnActionParameters &parameters) {
    menu()->trigger(actionId, parameters);
}

void QnNotificationsCollectionWidget::at_notificationCache_fileDownloaded(const QString &filename) {
    QString filePath = context()->instance<QnAppServerNotificationCache>()->getFullPath(filename);
    foreach (QnNotificationWidget* item, m_itemsByLoadingSound.values(filename)) {
        item->setSound(filePath, true);
    }
    m_itemsByLoadingSound.remove(filename);
}

void QnNotificationsCollectionWidget::cleanUpItem( QnNotificationWidget* item ) {
    foreach (QnSystemHealth::MessageType messageType, m_itemsByMessageType.keys(item))
        m_itemsByMessageType.remove(messageType, item);

    foreach (const QnUuid& ruleId, m_itemsByBusinessRuleId.keys(item))
        m_itemsByBusinessRuleId.remove(ruleId, item);

    foreach (QString soundPath, m_itemsByLoadingSound.keys(item))
        m_itemsByLoadingSound.remove(soundPath, item);
}
