#include "notifications_collection_widget.h"

#include <QApplication>
#include <QtGui/QGraphicsLinearLayout>

#include <business/business_strings_helper.h>

#include <camera/single_thumbnail_loader.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <client/client_settings.h>

#include <ui/common/geometry.h>
#include <ui/animation/opacity_animator.h>
#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/graphics/items/generic/particle_item.h>
#include <ui/graphics/items/generic/tool_tip_widget.h>
#include <ui/graphics/items/notifications/notification_item.h>
#include <ui/graphics/items/notifications/notification_list_widget.h>
#include <ui/style/skin.h>
#include <ui/style/resource_icon_cache.h>
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

    const char *itemResourcePropertyName = "_qn_itemResource";

} //anonymous namespace

QnBlinkingImageButtonWidget::QnBlinkingImageButtonWidget(QGraphicsItem *parent):
    base_type(parent),
    m_count(0),
    m_time(0)
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
    QnWorkbenchContextAware(context),
    m_blinker(NULL)
{
    m_headerWidget = new GraphicsWidget(this);

    qreal buttonSize = QApplication::style()->pixelMetric(QStyle::PM_ToolBarIconSize, NULL, NULL);

    QnImageButtonWidget *settingsButton = new QnImageButtonWidget(m_headerWidget);
    settingsButton->setIcon(qnSkin->icon("events/settings.png"));
    settingsButton->setToolTip(tr("Settings..."));
    settingsButton->setFixedSize(buttonSize);
    settingsButton->setCached(true);
    connect(settingsButton, SIGNAL(clicked()), this, SLOT(at_settingsButton_clicked()));

    QnImageButtonWidget *filterButton = new QnImageButtonWidget(m_headerWidget);
    filterButton->setIcon(qnSkin->icon("item/search.png"));
    filterButton->setToolTip(tr("Filter..."));
    filterButton->setFixedSize(buttonSize);
    filterButton->setCached(true);
    connect(filterButton, SIGNAL(clicked()), this, SLOT(at_filterButton_clicked()));

    QnImageButtonWidget *eventLogButton = new QnImageButtonWidget(m_headerWidget);
    eventLogButton->setIcon(qnSkin->icon("events/log.png"));
    eventLogButton->setToolTip(tr("Event Log"));
    eventLogButton->setFixedSize(buttonSize);
    eventLogButton->setCached(true);
    connect(eventLogButton, SIGNAL(clicked()), this, SLOT(at_eventLogButton_clicked()));

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
        connect(m_list, SIGNAL(itemCountChanged(int)), m_blinker, SLOT(setNotificationCount(int)));
        connect(m_list, SIGNAL(itemColorChanged(QColor)), m_blinker, SLOT(setColor(QColor)));
        m_blinker->setNotificationCount(m_list->itemCount());
    }
}

void QnNotificationsCollectionWidget::loadThumbnailForItem(QnNotificationItem *item, QnResourcePtr resource, qint64 usecsSinceEpoch)
{
    QnSingleThumbnailLoader* loader = QnSingleThumbnailLoader::newInstance(resource, usecsSinceEpoch, QSize(0, thumbnailHeight), item);
    item->setImageProvider(loader);
    //connect(loader, SIGNAL(finished()), loader, SLOT(deleteLater()));
}


void QnNotificationsCollectionWidget::showBusinessAction(const QnAbstractBusinessActionPtr &businessAction) {
    QnBusinessEventParameters params = businessAction->getRuntimeParams();
    int resourceId = params.getEventResourceId();
    QnResourcePtr resource = qnResPool->getResourceById(resourceId, QnResourcePool::AllResources);
    if (!resource)
        return;

    QnNotificationItem *item = new QnNotificationItem(m_list);

    item->setText(QnBusinessStringsHelper::shortEventDescription(params));
    item->setTooltipText(QnBusinessStringsHelper::longEventDescription(businessAction));

    QString name = resource->getName();

    BusinessEventType::Value eventType = params.getEventType();

    switch (eventType) {
    case BusinessEventType::Camera_Motion: {
        item->setColorLevel(QnNotificationItem::Common);
        item->addActionButton(
            qnResIconCache->icon(resource->flags(), resource->getStatus()),
            tr("Browse Archive"),
            Qn::OpenInNewLayoutAction,
            QnActionParameters(resource).withArgument(Qn::ItemTimeRole, params.getEventTimestamp()/1000)
        );
        loadThumbnailForItem(item, resource, params.getEventTimestamp());
        break;
    }
    case BusinessEventType::Camera_Input: {
        item->setColorLevel(QnNotificationItem::Common);
        item->addActionButton(
            qnResIconCache->icon(resource->flags(), resource->getStatus()),
            tr("Open Camera"),
            Qn::OpenInNewLayoutAction,
            QnActionParameters(resource)
        );
        loadThumbnailForItem(item, resource);
        break;
    }
    case BusinessEventType::Camera_Disconnect: {
        item->setColorLevel(QnNotificationItem::Important);
/*        item->addActionButton(
            qnResIconCache->icon(resource->flags(), resource->getStatus()),
            tr("Open Camera"),
            Qn::OpenInNewLayoutAction,
            QnActionParameters(resource)
        );*/
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
        item->setColorLevel(QnNotificationItem::Important);
      /*  item->addActionButton(
            qnResIconCache->icon(resource->flags(), resource->getStatus()),
            tr("Open Monitor"),
            Qn::OpenInNewLayoutAction,
            QnActionParameters(resource)
        );*/
        item->addActionButton(
            qnSkin->icon("events/storage.png"),
            tr("Server settings"),
            Qn::ServerSettingsAction,
            QnActionParameters(resource)
        );
        break;
    }
    case BusinessEventType::Network_Issue:{
        item->setColorLevel(QnNotificationItem::Important);
       /* item->addActionButton(
            qnResIconCache->icon(resource->flags(), resource->getStatus()),
            tr("Open Camera"),
            Qn::OpenInNewLayoutAction,
            QnActionParameters(resource)
        );*/
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
        item->setColorLevel(QnNotificationItem::Critical);
        QString webPageAddress = params.getSource();

        item->addActionButton(
            qnResIconCache->icon(resource->flags(), resource->getStatus()),
            tr("Open camera web page..."),
            Qn::BrowseUrlAction,
            QnActionParameters().withArgument(Qn::UrlRole, webPageAddress)
        );

        break;
    }
    case BusinessEventType::MediaServer_Failure: {
        item->setColorLevel(QnNotificationItem::Critical);
        item->addActionButton(
            qnResIconCache->icon(resource->flags(), resource->getStatus()),
            tr("Settings"),
            Qn::ServerSettingsAction,
            QnActionParameters(resource)
        );

        item->setText(tr("Failure on %1.").arg(name));
        break;
    }
    case BusinessEventType::MediaServer_Conflict: {
        item->setColorLevel(QnNotificationItem::Critical);
        item->addActionButton(
            qnResIconCache->icon(resource->flags(), resource->getStatus()),
            tr("Description"),
            Qn::MessageBoxAction,
            QnActionParameters().
                withArgument(Qn::TitleRole, tr("Information")).
                withArgument(Qn::TextRole, tr("There is another mediaserver in your network that watches your cameras."))
        );
        break;
    }
    default:
        break;
    }

    connect(item, SIGNAL(actionTriggered(Qn::ActionId, const QnActionParameters&)), this, SLOT(at_item_actionTriggered(Qn::ActionId, const QnActionParameters&)));
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

    QString name = resource ? resource->getName() : QString();

    item->setColorLevel(QnNotificationItem::System);

    switch (message) {
    case QnSystemHealth::EmailIsEmpty:
        item->addActionButton(
            qnSkin->icon("events/email.png"),
            tr("User Settings"),
            Qn::UserSettingsAction,
            QnActionParameters(context()->user()).withArgument(Qn::FocusElementRole, QString(QLatin1String("email")))
        );
        //default text
        break;
    case QnSystemHealth::NoLicenses:
        item->addActionButton(
            qnSkin->icon("events/license.png"),
            tr("Licenses"),
            Qn::PreferencesLicensesTabAction
        );
        //default text
        break;
    case QnSystemHealth::SmtpIsNotSet:
        item->addActionButton(
            qnSkin->icon("events/smtp.png"),
            tr("SMTP Settings"),
            Qn::PreferencesServerTabAction
        );
        //default text
        break;
    case QnSystemHealth::UsersEmailIsEmpty:
        item->addActionButton(
            qnSkin->icon("events/email.png"),
            tr("User Settings"),
            Qn::UserSettingsAction,
            QnActionParameters(resource).withArgument(Qn::FocusElementRole, QString(QLatin1String("email")))
        );
        item->setText(tr("E-Mail address is not set for user %1.").arg(name));
        break;
    case QnSystemHealth::ConnectionLost:
        item->addActionButton(
            qnSkin->icon("events/connection.png"),
            tr("Connect to server"),
            Qn::ConnectToServerAction
        );
        //default text
        break;
    case QnSystemHealth::EmailSendError:
        item->addActionButton(
            qnSkin->icon("events/email.png"),
            tr("SMTP Settings"),
            Qn::PreferencesServerTabAction
        );
        //default text
        break;
    case QnSystemHealth::StoragesNotConfigured:
        item->addActionButton(
            qnSkin->icon("events/storage.png"),
            tr("Server settings"),
            Qn::ServerSettingsAction,
            QnActionParameters(resource)
        );
        item->setText(tr("Storages are not configured on %1.").arg(name));
        break;
    case QnSystemHealth::StoragesAreFull:
        item->addActionButton(
            qnSkin->icon("events/storage.png"),
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

    item->setProperty(itemResourcePropertyName, QVariant::fromValue<QnResourcePtr>(resource));
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
    for (int i = 0; i < QnSystemHealth::MessageTypeCount; i++) {
        QnSystemHealth::MessageType message = QnSystemHealth::MessageType(i);
        if (m_itemsByMessageType.remove(message, item) > 0)
            break;
    }
    delete item;
}

void QnNotificationsCollectionWidget::at_item_actionTriggered(Qn::ActionId actionId, const QnActionParameters &parameters) {
    menu()->trigger(actionId, parameters);
}

