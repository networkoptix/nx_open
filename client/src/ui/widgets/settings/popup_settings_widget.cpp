#include "popup_settings_widget.h"
#include "ui_popup_settings_widget.h"

#include <client/client_settings.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>

#include <business/events/abstract_business_event.h>
#include <business/business_strings_helper.h>

#include <health/system_health.h>

#include <ui/workbench/handlers/workbench_notifications_handler.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workbench/workbench_context.h>

QnPopupSettingsWidget::QnPopupSettingsWidget(QWidget *parent) :
    QWidget(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnPopupSettingsWidget)
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::SystemSettings_Notifications_Help);

    for (int i = 0; i < BusinessEventType::Count; i++) {
        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(QnBusinessStringsHelper::eventName(BusinessEventType::Value(i)));
        ui->businessEventsLayout->addWidget(checkbox);
        m_businessRulesCheckBoxes << checkbox;
    }

    for (int i = 0; i < QnSystemHealth::MessageTypeCount; i++) {
        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(QnSystemHealthStringsHelper::messageTitle(QnSystemHealth::MessageType(i)));
        ui->systemHealthLayout->addWidget(checkbox);
        m_systemHealthCheckBoxes << checkbox;
    }

    m_showBusinessEventsHelper = this->context()->instance<QnShowBusinessEventsHelper>();

    connect(ui->showAllCheckBox,        SIGNAL(toggled(bool)),                              this,   SLOT(at_showAllCheckBox_toggled(bool)));
    connect(this->context(),            SIGNAL(userChanged(const QnUserResourcePtr &)),     this,   SLOT(at_context_userChanged()));
    connect(m_showBusinessEventsHelper, SIGNAL(valueChanged(quint64)),                      this,   SLOT(at_showBusinessEvents_valueChanged(quint64)));
}

QnPopupSettingsWidget::~QnPopupSettingsWidget()
{
}

void QnPopupSettingsWidget::updateFromSettings() {
    at_showBusinessEvents_valueChanged(m_showBusinessEventsHelper->value());
}

void QnPopupSettingsWidget::submitToSettings() {
    quint64 eventsShown = m_showBusinessEventsHelper->value();
    quint64 eventsFlag = 1;
    for (int i = 0; i < BusinessEventType::Count; i++) {
        if (m_businessRulesCheckBoxes[i]->isChecked() || ui->showAllCheckBox->isChecked()) {
            eventsShown |= eventsFlag;
        } else {
            eventsShown &= ~eventsFlag;
        }
        eventsFlag = eventsFlag << 1;
    }
    m_showBusinessEventsHelper->setValue(eventsShown);

    quint64 healthShown = qnSettings->popupSystemHealth();
    quint64 healthFlag = 1;
    for (int i = 0; i < QnSystemHealth::MessageTypeCount; i++) {
        if (m_systemHealthCheckBoxes[i]->isChecked() || ui->showAllCheckBox->isChecked()) {
            healthShown |= healthFlag;
        } else {
            healthShown &= ~healthFlag;
        }
        healthFlag = healthFlag << 1;
    }
    qnSettings->setPopupSystemHealth(healthShown);
}

void QnPopupSettingsWidget::at_showAllCheckBox_toggled(bool checked) {
    // TODO: #GDM also update checked state!
    // TODO: #GDM maybe tristate for 'show all' checkbox would be better.
    foreach (QCheckBox* checkbox, m_businessRulesCheckBoxes) {
        checkbox->setEnabled(!checked);
    }

    foreach (QCheckBox* checkbox, m_systemHealthCheckBoxes) {
        checkbox->setEnabled(!checked);
    }
}

void QnPopupSettingsWidget::at_showBusinessEvents_valueChanged(quint64 value) {
    bool all = true;

    quint64 healthShown = qnSettings->popupSystemHealth();
    quint64 healthFlag = 1;
    for (int i = 0; i < QnSystemHealth::MessageTypeCount; i++) {
        bool checked = healthShown & healthFlag;
        m_systemHealthCheckBoxes[i]->setChecked(checked);
        all = all && checked;
        healthFlag = healthFlag << 1;
    }

    quint64 eventsShown = value;
    quint64 eventsFlag = 1;
    for (int i = 0; i < BusinessEventType::Count; i++) {
        bool checked = eventsShown & eventsFlag;
        m_businessRulesCheckBoxes[i]->setChecked(checked);
        all = all && checked;
        eventsFlag = eventsFlag << 1;
    }

    ui->showAllCheckBox->setChecked(all);
}


void QnPopupSettingsWidget::at_context_userChanged() {
    m_showBusinessEventsHelper->setResource(context()->user());
}
