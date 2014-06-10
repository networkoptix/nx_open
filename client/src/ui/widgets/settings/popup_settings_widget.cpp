#include "popup_settings_widget.h"
#include "ui_popup_settings_widget.h"

#include <client/client_settings.h>

#include <api/app_server_connection.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>

#include <business/events/abstract_business_event.h>
#include <business/business_strings_helper.h>

#include <health/system_health.h>

#include <ui/workbench/handlers/workbench_notifications_handler.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workbench/workbench_context.h>

#include <utils/resource_property_adaptors.h>

//TODO: #GDM #Common handle user changing here

QnPopupSettingsWidget::QnPopupSettingsWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::PopupSettingsWidget),
    m_adaptor(new QnBusinessEventsFilterResourcePropertyAdaptor(this))
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::SystemSettings_Notifications_Help);

    for (int i = 1; i < QnBusiness::EventCount; i++) {
        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(QnBusinessStringsHelper::eventName(QnBusiness::EventType(i)));
        ui->businessEventsLayout->addWidget(checkbox);
        m_businessRulesCheckBoxes << checkbox;
    }

    for (int i = 0; i < QnSystemHealth::MessageTypeCount; i++) {
        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(QnSystemHealthStringsHelper::messageTitle(QnSystemHealth::MessageType(i)));
        ui->systemHealthLayout->addWidget(checkbox);
        m_systemHealthCheckBoxes << checkbox;
    }

    m_adaptor->setResource(context()->user()); // TODO: #Elric listen to changes?

    connect(ui->showAllCheckBox,    &QCheckBox::toggled,                                this,   &QnPopupSettingsWidget::at_showAllCheckBox_toggled);
    connect(m_adaptor,              &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnPopupSettingsWidget::at_showBusinessEvents_valueChanged);
}

QnPopupSettingsWidget::~QnPopupSettingsWidget()
{
}

void QnPopupSettingsWidget::updateFromSettings() {
    quint64 healthShown = qnSettings->popupSystemHealth();
    quint64 healthFlag = 1;
    for (int i = 0; i < QnSystemHealth::MessageTypeCount; i++) {
        bool checked = healthShown & healthFlag;
        m_systemHealthCheckBoxes[i]->setChecked(checked);
        healthFlag = healthFlag << 1;
    }

    at_showBusinessEvents_valueChanged();
}

void QnPopupSettingsWidget::submitToSettings() {
    if (context()->user()) {
        quint64 eventsShown = 0xFFFFFFFFFFFFFFFFull;
        if (!ui->showAllCheckBox->isChecked()) {
            quint64 eventsFlag = 1;
            for (int i = 1; i < QnBusiness::EventCount; i++) {
                if (!m_businessRulesCheckBoxes[i-1]->isChecked())
                    eventsShown &= ~eventsFlag;
                eventsFlag = eventsFlag << 1;
            }
        }

        m_adaptor->setValue(eventsShown);
    }

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
    // TODO: #GDM #Common also update checked state!
    // TODO: #GDM #Common maybe tristate for 'show all' checkbox would be better.
    foreach (QCheckBox* checkbox, m_businessRulesCheckBoxes) {
        checkbox->setEnabled(!checked);
    }

    foreach (QCheckBox* checkbox, m_systemHealthCheckBoxes) {
        checkbox->setEnabled(!checked);
    }
}

void QnPopupSettingsWidget::at_showBusinessEvents_valueChanged() {
    bool all = true;
    if (!ui->showAllCheckBox->isChecked()) {
        foreach (QCheckBox* systemHealthCheckbox, m_systemHealthCheckBoxes) {
            if (!systemHealthCheckbox->isChecked()) {
                all = false;
                break;
            }
        }
    }

    quint64 eventsShown = m_adaptor->value();
    quint64 eventsFlag = 1;
    for (int i = 1; i < QnBusiness::EventCount; i++) {
        bool checked = eventsShown & eventsFlag;
        m_businessRulesCheckBoxes[i-1]->setChecked(checked);
        all = all && checked;
        eventsFlag = eventsFlag << 1;
    }

    ui->showAllCheckBox->setChecked(all);
}
