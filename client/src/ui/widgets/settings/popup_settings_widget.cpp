#include "popup_settings_widget.h"
#include "ui_popup_settings_widget.h"

#include <utils/settings.h>

#include <business/events/abstract_business_event.h>

#include <health/system_health.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

QnPopupSettingsWidget::QnPopupSettingsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnPopupSettingsWidget)
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::SystemSettings_Notifications_Help);

    for (int i = 0; i < BusinessEventType::Count; i++) {
        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(BusinessEventType::toString(BusinessEventType::Value(i)));
        ui->businessEventsLayout->addWidget(checkbox);
        m_businessRulesCheckBoxes << checkbox;
    }

    for (int i = 0; i < QnSystemHealth::MessageTypeCount; i++) {
        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(QnSystemHealth::toString(QnSystemHealth::MessageType(i)));
        ui->systemHealthLayout->addWidget(checkbox);
        m_systemHealthCheckBoxes << checkbox;
    }

    connect(ui->showAllCheckBox, SIGNAL(toggled(bool)), this, SLOT(at_showAllCheckBox_toggled(bool)));
}

QnPopupSettingsWidget::~QnPopupSettingsWidget()
{
}

void QnPopupSettingsWidget::updateFromSettings(QnClientSettings *settings) {
    bool all = true;

    quint64 eventsShown = settings->popupBusinessEvents();
    quint64 eventsFlag = 1;
    for (int i = 0; i < BusinessEventType::Count; i++) {
        bool checked = eventsShown & eventsFlag;
        m_businessRulesCheckBoxes[i]->setChecked(checked);
        all = all && checked;
        eventsFlag = eventsFlag << 1;
    }

    quint64 healthShown = settings->popupSystemHealth();
    quint64 healthFlag = 1;
    for (int i = 0; i < QnSystemHealth::MessageTypeCount; i++) {
        bool checked = healthShown & healthFlag;
        m_systemHealthCheckBoxes[i]->setChecked(checked);
        all = all && checked;
        healthFlag = healthFlag << 1;
    }

    ui->showAllCheckBox->setChecked(all);
}

void QnPopupSettingsWidget::submitToSettings(QnClientSettings *settings) {
    quint64 eventsShown = settings->popupBusinessEvents();
    quint64 eventsFlag = 1;
    for (int i = 0; i < BusinessEventType::Count; i++) {
        if (m_businessRulesCheckBoxes[i]->isChecked() || ui->showAllCheckBox->isChecked()) {
            eventsShown |= eventsFlag;
        } else {
            eventsShown &= ~eventsFlag;
        }
        eventsFlag = eventsFlag << 1;
    }
    settings->setPopupBusinessEvents(eventsShown);

    quint64 healthShown = settings->popupSystemHealth();
    quint64 healthFlag = 1;
    for (int i = 0; i < QnSystemHealth::MessageTypeCount; i++) {
        if (m_systemHealthCheckBoxes[i]->isChecked() || ui->showAllCheckBox->isChecked()) {
            healthShown |= healthFlag;
        } else {
            healthShown &= ~healthFlag;
        }
        healthFlag = healthFlag << 1;
    }
    settings->setPopupSystemHealth(healthShown);
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
