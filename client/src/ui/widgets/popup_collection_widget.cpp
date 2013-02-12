#include "popup_collection_widget.h"
#include "ui_popup_collection_widget.h"

#include <business/actions/popup_business_action.h>

#include <core/resource/resource.h>
#include <core/resource_managment/resource_pool.h>

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

    // TODO: Evil! Layout code does not belong here.
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

    if (!(qnSettings->shownPopups() & (1 << eventType))) {
        qDebug() << "popup received, ignoring" << BusinessEventType::toString(eventType);
        return false;
    }

    int id = QnBusinessEventRuntime::getEventResourceId(businessAction->getRuntimeParams());
    QnResourcePtr res = qnResPool->getResourceById(id, QnResourcePool::rfAllResources);
    QString resource = res ? res->getName() : QString();

    qDebug() << "popup received" << BusinessEventType::toString(eventType) << "from" << resource << "(" << id << ")";

    if (m_widgetsByType.contains(eventType)) {
        QnPopupWidget* pw = m_widgetsByType[eventType];
        pw->addBusinessAction(businessAction);
    } else {
        QnPopupWidget* pw = new QnPopupWidget(this);
        if (!pw->addBusinessAction(businessAction))
            return false;
        ui->verticalLayout->addWidget(pw);
        m_widgetsByType[eventType] = pw;
        connect(pw, SIGNAL(closed(BusinessEventType::Value, bool)), this, SLOT(at_widget_closed(BusinessEventType::Value, bool)));
    }

    return true;
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
    QSize parentSize = parentWidget()->size();
    QSize size = this->size();
    move(parentSize.width() - size.width(), parentSize.height() - size.height());
}

void QnPopupCollectionWidget::at_widget_closed(BusinessEventType::Value eventType, bool ignore) {
    if (ignore)
        qnSettings->setShownPopups(qnSettings->shownPopups() & ~(1 << eventType));

    if (!m_widgetsByType.contains(eventType))
        return;

    QWidget* w = m_widgetsByType[eventType];
    ui->verticalLayout->removeWidget(w);
    m_widgetsByType.remove(eventType);

    if (ui->verticalLayout->count() == 0)
        hide();
}
