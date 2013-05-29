#include "popup_collection_widget.h"
#include "ui_popup_collection_widget.h"

#include <business/business_action_parameters.h>

#include <client/client_settings.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <ui/animation/opacity_animator.h>
#include <ui/widgets/popups/business_event_popup_widget.h>
#include <ui/widgets/popups/system_health_popup_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/kvpair_usage_helper.h>

QnPopupCollectionWidget::QnPopupCollectionWidget(QnWorkbenchContext* context):
    base_type(NULL),
    QnWorkbenchContextAware(context),
    ui(new Ui::QnPopupCollectionWidget)
{
    init();
}

QnPopupCollectionWidget::QnPopupCollectionWidget(QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnPopupCollectionWidget)
{
    init();
}

QnPopupCollectionWidget::~QnPopupCollectionWidget()
{
}

void QnPopupCollectionWidget::init() {
    ui->setupUi(this);

    m_showBusinessEventsHelper = new QnUint64KvPairUsageHelper(
                this->context()->user(),
                QLatin1String("showBusinessEvents"),            //TODO: #GDM move out common consts
                0xFFFFFFFFFFFFFFFFull,                          //TODO: #GDM move out common consts
                this);

    connect(ui->postponeAllButton,  SIGNAL(clicked()), this, SLOT(at_postponeAllButton_clicked()));
    connect(this->context(),        SIGNAL(userChanged(const QnUserResourcePtr &)), this, SLOT(at_context_userChanged()));
}

bool QnPopupCollectionWidget::addBusinessAction(const QnAbstractBusinessActionPtr &businessAction) {
    if (businessAction->actionType() != BusinessActionType::ShowPopup)
        return false;

    //TODO: #GDM check if camera is visible to us
    QnBusinessActionParameters::UserGroup userGroup = businessAction->getParams().getUserGroup();
    if (userGroup == QnBusinessActionParameters::AdminOnly
            && !(accessController()->globalPermissions() & Qn::GlobalProtectedPermission)) {
        qDebug() << "popup for admins received, we are not admin";
        return false;
    }

    QnBusinessEventParameters params = businessAction->getRuntimeParams();
    BusinessEventType::Value eventType = params.getEventType();

    if (eventType >= BusinessEventType::UserDefined)
        return false;

    int healthMessage = eventType - BusinessEventType::SystemHealthMessage;
    if (healthMessage >= 0) {
        QnResourceList resources;

        int resourceId = params.getEventResourceId();
        QnResourcePtr resource = qnResPool->getResourceById(resourceId, QnResourcePool::rfAllResources);
        if (resource)
            resources << resource;

        return addSystemHealthEvent(QnSystemHealth::MessageType(healthMessage), resources);
    }

    if (!(m_showBusinessEventsHelper->value() & (1 << eventType))) {
        qDebug() << "popup received, ignoring" << BusinessEventType::toString(eventType);
        return false;
    }

    int id = businessAction->getRuntimeParams().getEventResourceId();
    QnResourcePtr res = qnResPool->getResourceById(id, QnResourcePool::rfAllResources);
    QString resource = res ? res->getName() : QString();

    qDebug() << "popup received" << eventType << BusinessEventType::toString(eventType) << "from" << resource << "(" << id << ")";

    if (m_businessEventWidgets.contains(eventType)) {
        QnBusinessEventPopupWidget* pw = m_businessEventWidgets[eventType];
        pw->addBusinessAction(businessAction);
    } else {
        QnBusinessEventPopupWidget* pw = new QnBusinessEventPopupWidget(ui->verticalWidget);
        if (!pw->addBusinessAction(businessAction))
            return false;
        ui->verticalLayout->insertWidget(0, pw);
        m_businessEventWidgets[eventType] = pw;
        connect(pw, SIGNAL(closed(BusinessEventType::Value, bool)), this, SLOT(at_businessEventWidget_closed(BusinessEventType::Value, bool)));
    }

    return true;
}

bool QnPopupCollectionWidget::addSystemHealthEvent(QnSystemHealth::MessageType message) {
    return addSystemHealthEvent(message, QnResourceList());
}

bool QnPopupCollectionWidget::addSystemHealthEvent(QnSystemHealth::MessageType message, const QnResourceList &resources) {
    if (!(qnSettings->popupSystemHealth() & (1 << message)))
        return false;

    if (m_systemHealthWidgets.contains(message)) {
        QnSystemHealthPopupWidget* pw = m_systemHealthWidgets[message];
        pw->show();
    } else {
        QnSystemHealthPopupWidget* pw = new QnSystemHealthPopupWidget(ui->verticalWidget);
        if (!pw->showSystemHealthMessage(message, resources))
            return false;
        ui->verticalLayout->addWidget(pw);
        m_systemHealthWidgets[message] = pw;
        connect(pw, SIGNAL(closed(QnSystemHealth::MessageType, bool)), this, SLOT(at_systemHealthWidget_closed(QnSystemHealth::MessageType, bool)));
    }

    return true;
}

void QnPopupCollectionWidget::clear(bool animate) {
    while (!ui->verticalLayout->isEmpty()) {
        QLayoutItem* item = ui->verticalLayout->itemAt(0);
        if (item->widget())
            item->widget()->hide();
        ui->verticalLayout->removeItem(item);
    }
    m_businessEventWidgets.clear();
    m_systemHealthWidgets.clear();
    hide();
}

bool QnPopupCollectionWidget::isEmpty() const {
    return ui->verticalLayout->isEmpty();
}

void QnPopupCollectionWidget::at_businessEventWidget_closed(BusinessEventType::Value eventType, bool ignore) {
    if (ignore)
        m_showBusinessEventsHelper->setValue(m_showBusinessEventsHelper->value() & ~(1 << eventType));

    if (!m_businessEventWidgets.contains(eventType))
        return;

    QWidget* w = m_businessEventWidgets[eventType];
    ui->verticalLayout->removeWidget(w);
    m_businessEventWidgets.remove(eventType);
    w->hide();
    w->deleteLater();

    if (ui->verticalLayout->count() == 0)
        hide();
}

void QnPopupCollectionWidget::at_systemHealthWidget_closed(QnSystemHealth::MessageType message, bool ignore) {
    if (ignore)
        qnSettings->setPopupSystemHealth(qnSettings->popupSystemHealth() & ~(1 << message));

    if (!m_systemHealthWidgets.contains(message))
        return;

    QWidget* w = m_systemHealthWidgets[message];
    ui->verticalLayout->removeWidget(w);
    m_systemHealthWidgets.remove(message);
    w->hide();
    w->deleteLater();

    if (ui->verticalLayout->count() == 0)
        hide();
}

void QnPopupCollectionWidget::at_postponeAllButton_clicked() {
    clear();
}

void QnPopupCollectionWidget::at_minimizeButton_clicked() {
    hide();
}

void QnPopupCollectionWidget::at_context_userChanged() {
    m_showBusinessEventsHelper->setResource(context()->user());
}

