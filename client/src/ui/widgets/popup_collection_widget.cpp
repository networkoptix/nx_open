#include "popup_collection_widget.h"
#include "ui_popup_collection_widget.h"

#include <ui/widgets/popup_widget.h>
#include <events/popup_business_action.h>

QnPopupCollectionWidget::QnPopupCollectionWidget(QWidget *parent) :
    QWidget(parent, Qt::Popup),
    ui(new Ui::QnPopupCollectionWidget)
{
    ui->setupUi(this);

    m_adding = true; //debug variable
}

QnPopupCollectionWidget::~QnPopupCollectionWidget()
{
    delete ui;
}

void QnPopupCollectionWidget::addExample() {
    if (m_adding) {
        QnPopupWidget* w = new QnPopupWidget(this);
        ui->verticalLayout->addWidget(w);
        w = new QnPopupWidget(this);
        ui->verticalLayout->addWidget(w);
        w = new QnPopupWidget(this);
        ui->verticalLayout->addWidget(w);
        m_adding = false;
    } else {
        ui->verticalLayout->removeItem(ui->verticalLayout->itemAt(0));
        m_adding = ui->verticalLayout->count() == 0;
    }
}

void QnPopupCollectionWidget::addBusinessAction(const QnAbstractBusinessActionPtr &businessAction) {
    //TODO: #GDM check admins
    int group = BusinessActionParameters::getUserGroup(businessAction->getParams());
    // now 1 is Admins Only

    QnBusinessParams params = businessAction->getRuntimeParams();
    BusinessEventType::Value eventType = QnBusinessEventRuntime::getEventType(params);

    if (m_ignore.contains(eventType) && m_ignore[eventType])
        return;

    //TODO: get event type from runtime params
    if (QWidget* w = m_widgetsByType[eventType]) {
        QnPopupWidget* pw = dynamic_cast<QnPopupWidget*>(w);
        pw->addBusinessAction(businessAction);
        return;
    }
    QnPopupWidget* pw = new QnPopupWidget(this);
    ui->verticalLayout->addWidget(pw);
    m_widgetsByType[eventType] = pw;
    pw->addBusinessAction(businessAction);
    connect(pw, SIGNAL(closed(BusinessEventType::Value, bool)), this, SLOT(at_widget_closed(BusinessEventType::Value, bool)));

    if (isVisible())
        updatePosition();
}

void QnPopupCollectionWidget::showEvent(QShowEvent *event) {
    updatePosition();
    base_type::showEvent(event);
}

void QnPopupCollectionWidget::updatePosition() {
    QRect pgeom = static_cast<QWidget *>(parent())->geometry();
    QRect geom = geometry();
    setGeometry(pgeom.left() + pgeom.width() - geom.width(), pgeom.top() + pgeom.height() - geom.height(), geom.width(), geom.height());
}

void QnPopupCollectionWidget::at_widget_closed(BusinessEventType::Value eventType, bool ignore) {
    m_ignore[eventType] = ignore;

    if (!m_widgetsByType.contains(eventType))
        return;

    QWidget* w = m_widgetsByType[eventType];
    ui->verticalLayout->removeWidget(w);
    m_widgetsByType.remove(eventType);
}
