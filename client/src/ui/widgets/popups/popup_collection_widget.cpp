#include "popup_collection_widget.h"
#include "ui_popup_collection_widget.h"

#include <business/actions/popup_business_action.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <ui/widgets/popups/business_event_popup_widget.h>
#include <ui/widgets/popups/system_health_popup_widget.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/settings.h>
#include <utils/common/event_processors.h>

QnPopupCollectionWidget::QnPopupCollectionWidget(QWidget *parent, QnWorkbenchContext *context):
    base_type(parent),
    QnWorkbenchContextAware(parent, context),
    ui(new Ui::QnPopupCollectionWidget)
{
    ui->setupUi(this);

    this->setAutoFillBackground(true);

    // TODO: #GDM Evil! Layout code does not belong here.
    // Layout must be done by widget's parent, not the widget itself.
    QnSingleEventSignalizer *resizeSignalizer = new QnSingleEventSignalizer(this);
    resizeSignalizer->setEventType(QEvent::Resize);
    parent->installEventFilter(resizeSignalizer);
    connect(resizeSignalizer, SIGNAL(activated(QObject *, QEvent *)), this, SLOT(updatePosition()));
}

QnPopupCollectionWidget::~QnPopupCollectionWidget()
{
}

bool QnPopupCollectionWidget::addBusinessAction(const QnAbstractBusinessActionPtr &businessAction) {
    if (businessAction->actionType() != BusinessActionType::BA_ShowPopup)
        return false;

    //TODO: #GDM check if camera is visible to us
    int group = BusinessActionParameters::getUserGroup(businessAction->getParams());
    if (group > 0 && !(accessController()->globalPermissions() & Qn::GlobalProtectedPermission)) {
        qDebug() << "popup for admins received, we are not admin";
        return false;
    }
    // now 1 is Admins Only

    QnBusinessParams params = businessAction->getRuntimeParams();
    BusinessEventType::Value eventType = QnBusinessEventRuntime::getEventType(params);

    if (eventType >= BusinessEventType::BE_UserDefined)
        return false;

    int healthMessage = eventType - BusinessEventType::BE_SystemHealthMessage;
    if (healthMessage >= 0) {
        QnResourceList resources;

        int resourceId = QnBusinessEventRuntime::getEventResourceId(params);
        QnResourcePtr resource = qnResPool->getResourceById(resourceId, QnResourcePool::rfAllResources);
        if (resource)
            resources << resource;

        return addSystemHealthEvent(QnSystemHealth::MessageType(healthMessage), resources);
    }

    if (!(qnSettings->popupBusinessEvents() & (1 << eventType))) {
        qDebug() << "popup received, ignoring" << BusinessEventType::toString(eventType);
        return false;
    }

    int id = QnBusinessEventRuntime::getEventResourceId(businessAction->getRuntimeParams());
    QnResourcePtr res = qnResPool->getResourceById(id, QnResourcePool::rfAllResources);
    QString resource = res ? res->getName() : QString();

    qDebug() << "popup received" << eventType << BusinessEventType::toString(eventType) << "from" << resource << "(" << id << ")";

    if (m_businessEventWidgets.contains(eventType)) {
        QnBusinessEventPopupWidget* pw = m_businessEventWidgets[eventType];
        pw->addBusinessAction(businessAction);
    } else {
        QnBusinessEventPopupWidget* pw = new QnBusinessEventPopupWidget(this);
        if (!pw->addBusinessAction(businessAction))
            return false;
        ui->verticalLayout->insertWidget(0, pw);
        m_businessEventWidgets[eventType] = pw;
        connect(pw, SIGNAL(closed(BusinessEventType::Value, bool)), this, SLOT(at_businessEventWidget_closed(BusinessEventType::Value, bool)));
    }

    if (!isVisible())
        action(Qn::TogglePopupsAction)->setChecked(true);
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
        QnSystemHealthPopupWidget* pw = new QnSystemHealthPopupWidget(this);
        if (!pw->showSystemHealthMessage(message, resources))
            return false;
        ui->verticalLayout->addWidget(pw);
        m_systemHealthWidgets[message] = pw;
        connect(pw, SIGNAL(closed(QnSystemHealth::MessageType, bool)), this, SLOT(at_systemHealthWidget_closed(QnSystemHealth::MessageType, bool)));
    }

    if (!isVisible())
        action(Qn::TogglePopupsAction)->setChecked(true);
    return true;
}

void QnPopupCollectionWidget::clear() {
    while (!ui->verticalLayout->isEmpty()) {
        QLayoutItem* item = ui->verticalLayout->itemAt(0);
        if (item->widget())
            item->widget()->hide();
        ui->verticalLayout->removeItem(item);
    }
    m_businessEventWidgets.clear();
    m_systemHealthWidgets.clear();
    action(Qn::TogglePopupsAction)->setChecked(false);
    hide();
}

bool QnPopupCollectionWidget::isEmpty() const {
    return ui->verticalLayout->isEmpty();
}

void QnPopupCollectionWidget::showEvent(QShowEvent *event) {
    base_type::showEvent(event);
    updatePosition();
}

void QnPopupCollectionWidget::resizeEvent(QResizeEvent *event) {
    base_type::resizeEvent(event);
    updatePosition();
}

void QnPopupCollectionWidget::updatePosition() {
    //TODO: #GDM will not be used when will be placed on scene
    const int offset = 3;

    QSize parentSize = parentWidget()->size();
    QSize size = this->size();
    move(parentSize.width() - size.width() - offset, parentSize.height() - size.height() - offset);
}

void QnPopupCollectionWidget::at_businessEventWidget_closed(BusinessEventType::Value eventType, bool ignore) {
    if (ignore)
        qnSettings->setPopupBusinessEvents(qnSettings->popupBusinessEvents() & ~(1 << eventType));

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
