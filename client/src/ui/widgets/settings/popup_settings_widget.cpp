#include "popup_settings_widget.h"
#include "ui_popup_settings_widget.h"

#include <client/client_settings.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>

#include <business/events/abstract_business_event.h>
#include <business/business_strings_helper.h>

#include <health/system_health_helper.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workbench/workbench_context.h>

#include <utils/resource_property_adaptors.h>

QnPopupSettingsWidget::QnPopupSettingsWidget(QWidget *parent) 
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , ui(new Ui::PopupSettingsWidget)
    , m_businessRulesCheckBoxes()
    , m_systemHealthCheckBoxes()
    , m_adaptor(new QnBusinessEventsFilterResourcePropertyAdaptor(this))
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::SystemSettings_Notifications_Help);

    for (QnBusiness::EventType eventType: QnBusiness::allEvents()) {
        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(QnBusinessStringsHelper::eventName(eventType));
        ui->businessEventsLayout->addWidget(checkbox);
        m_businessRulesCheckBoxes[eventType] = checkbox;
    }

    static_assert(QnSystemHealth::MessageTypeCount < 64, "We are packing messages to single quint64");
    for (int i = 0; i < QnSystemHealth::MessageTypeCount; i++) {
        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(QnSystemHealthStringsHelper::messageTitle(QnSystemHealth::MessageType(i)));
        ui->systemHealthLayout->addWidget(checkbox);
        m_systemHealthCheckBoxes << checkbox;
    }

    connect(m_adaptor,              &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnPopupSettingsWidget::loadDataToUi);

    connect(ui->showAllCheckBox,    &QCheckBox::toggled,                                this,   [this](bool checked){
        // TODO: #GDM #Common also update checked state!
        // TODO: #GDM #Common maybe tristate for 'show all' checkbox would be better.
        for (QCheckBox* checkbox: m_businessRulesCheckBoxes) {
            checkbox->setEnabled(!checked);
        }

        for (QCheckBox* checkbox:  m_systemHealthCheckBoxes) {
            checkbox->setEnabled(!checked);
        }
    });
    
}

QnPopupSettingsWidget::~QnPopupSettingsWidget()
{
}

void QnPopupSettingsWidget::loadDataToUi() {
    bool all = true;

    quint64 healthShown = qnSettings->popupSystemHealth();
    quint64 healthFlag = 1;
    for (int i = 0; i < QnSystemHealth::MessageTypeCount; i++) {
        bool checked = ((healthShown & healthFlag) == healthFlag);
        m_systemHealthCheckBoxes[i]->setChecked(checked);
        healthFlag = healthFlag << 1;

        all &= checked;
    }

    if (context()->user())
        m_adaptor->setResource(context()->user());   

    QList<QnBusiness::EventType> watchedEvents = context()->user()
        ? m_adaptor->watchedEvents()
        : QnBusiness::allEvents();

    for (QnBusiness::EventType eventType: m_businessRulesCheckBoxes.keys()) {
        bool checked = watchedEvents.contains(eventType);
        m_businessRulesCheckBoxes[eventType]->setChecked(checked);
        all &= checked;
    }

    ui->showAllCheckBox->setChecked(all);

    ui->businessEventsGroupBox->setEnabled(context()->user());
}

void QnPopupSettingsWidget::applyChanges() {
    if (context()->user())
        m_adaptor->setWatchedEvents(watchedEvents());
    qnSettings->setPopupSystemHealth(watchedSystemHealth());
}

bool QnPopupSettingsWidget::hasChanges() const  {
    return qnSettings->popupSystemHealth() != watchedSystemHealth()
        || (context()->user() && m_adaptor->watchedEvents() != watchedEvents());
}

QList<QnBusiness::EventType> QnPopupSettingsWidget::watchedEvents() const {
    if (ui->showAllCheckBox->isChecked())
        return QnBusiness::allEvents();

    QList<QnBusiness::EventType> result;
    for (QnBusiness::EventType eventType: m_businessRulesCheckBoxes.keys())
        if (m_businessRulesCheckBoxes[eventType]->isChecked())
            result << eventType;
    return result;
}

quint64 QnPopupSettingsWidget::watchedSystemHealth() const {
    quint64 result = qnSettings->popupSystemHealth();
    quint64 healthFlag = 1;
    for (int i = 0; i < QnSystemHealth::MessageTypeCount; i++) {
        if (m_systemHealthCheckBoxes[i]->isChecked() || ui->showAllCheckBox->isChecked()) {
            result |= healthFlag;
        } else {
            result &= ~healthFlag;
        }
        healthFlag = healthFlag << 1;
    }
    return result;
}
