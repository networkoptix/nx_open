#include "popup_settings_widget.h"
#include "ui_popup_settings_widget.h"

#include <client/client_settings.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>

#include <business/events/abstract_business_event.h>

#include <health/system_health.h>

#include <utils/kvpair_usage_helper.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workbench/workbench_context.h>

QnPopupSettingsWidget::QnPopupSettingsWidget(QnWorkbenchContext *context, QWidget *parent) :
    QWidget(parent),
    QnWorkbenchContextAware(context),
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

    m_showBusinessEventsHelper = new QnKvPairUsageHelper(
                context->user(),
                QLatin1String("showBusinessEvents"),    //TODO: #GDM move out common consts
                0xFFFFFFFFFFFFFFFFull,                  //TODO: #GDM move out common consts
                this);

    m_showSystemHealthHelper = new QnKvPairUsageHelper(
                context->user(),
                QLatin1String("showSystemHealth"),      //TODO: #GDM move out common consts
                0xFFFFFFFFFFFFFFFFull,                  //TODO: #GDM move out common consts
                this);

    connect(ui->showAllCheckBox, SIGNAL(toggled(bool)),                             this,   SLOT(at_showAllCheckBox_toggled(bool)));
    connect(context,             SIGNAL(userChanged(const QnUserResourcePtr &)),    this,   SLOT(at_context_userChanged()));
    connect(m_showBusinessEventsHelper, SIGNAL(valueChanged(QString)),              this,   SLOT(at_showBusinessEvents_valueChanged(QString)));
    connect(m_showSystemHealthHelper, SIGNAL(valueChanged(QString)),                this,   SLOT(at_showSystemHealth_valueChanged(QString)));


    at_showBusinessEvents_valueChanged(QString());
    at_showSystemHealth_valueChanged(QString());
}

QnPopupSettingsWidget::~QnPopupSettingsWidget()
{
}

void QnPopupSettingsWidget::submit() {
    quint64 eventsShown = m_showBusinessEventsHelper->valueAsFlags();
    quint64 eventsFlag = 1;
    for (int i = 0; i < BusinessEventType::Count; i++) {
        if (m_businessRulesCheckBoxes[i]->isChecked() || ui->showAllCheckBox->isChecked()) {
            eventsShown |= eventsFlag;
        } else {
            eventsShown &= ~eventsFlag;
        }
        eventsFlag = eventsFlag << 1;
    }
    m_showBusinessEventsHelper->setFlagsValue(eventsShown);

    quint64 healthShown = m_showSystemHealthHelper->valueAsFlags();
    quint64 healthFlag = 1;
    for (int i = 0; i < QnSystemHealth::MessageTypeCount; i++) {
        if (m_systemHealthCheckBoxes[i]->isChecked() || ui->showAllCheckBox->isChecked()) {
            healthShown |= healthFlag;
        } else {
            healthShown &= ~healthFlag;
        }
        healthFlag = healthFlag << 1;
    }
    m_showSystemHealthHelper->setFlagsValue(healthShown);
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

void QnPopupSettingsWidget::at_showBusinessEvents_valueChanged(const QString &value) {
    Q_UNUSED(value)
    bool all = true;
    foreach (QCheckBox* c, m_systemHealthCheckBoxes) {
        all = all && c->isChecked();
    }

    quint64 eventsShown = m_showBusinessEventsHelper->valueAsFlags();
    quint64 eventsFlag = 1;
    for (int i = 0; i < BusinessEventType::Count; i++) {
        bool checked = eventsShown & eventsFlag;
        m_businessRulesCheckBoxes[i]->setChecked(checked);
        all = all && checked;
        eventsFlag = eventsFlag << 1;
    }

//    ui->showAllCheckBox->setChecked(all);
}

void QnPopupSettingsWidget::at_showSystemHealth_valueChanged(const QString &value) {
    Q_UNUSED(value)
    bool all = true;
    foreach (QCheckBox* c, m_businessRulesCheckBoxes) {
        all = all && c->isChecked();
    }

    quint64 healthShown = m_showSystemHealthHelper->valueAsFlags();
    quint64 healthFlag = 1;
    for (int i = 0; i < QnSystemHealth::MessageTypeCount; i++) {
        bool checked = healthShown & healthFlag;
        m_systemHealthCheckBoxes[i]->setChecked(checked);
        all = all && checked;
        healthFlag = healthFlag << 1;
    }

//    ui->showAllCheckBox->setChecked(all);
}

void QnPopupSettingsWidget::at_context_userChanged() {
    m_showBusinessEventsHelper->setResource(context()->user());
    m_showSystemHealthHelper->setResource(context()->user());
}
