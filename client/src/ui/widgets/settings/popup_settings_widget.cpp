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
#include <utils/common/scoped_value_rollback.h>

QnPopupSettingsWidget::QnPopupSettingsWidget(QWidget *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , ui(new Ui::PopupSettingsWidget)
    , m_businessRulesCheckBoxes()
    , m_systemHealthCheckBoxes()
    , m_adaptor(new QnBusinessEventsFilterResourcePropertyAdaptor(this))
    , m_updating(false)
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::SystemSettings_Notifications_Help);

    for (QnBusiness::EventType eventType: QnBusiness::allEvents()) {
        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(QnBusinessStringsHelper::eventName(eventType));
        ui->businessEventsLayout->addWidget(checkbox);
        m_businessRulesCheckBoxes[eventType] = checkbox;
    }

    static_assert(QnSystemHealth::Count < 64, "We are packing messages to single quint64");

    for (int i = 0; i < QnSystemHealth::Count; i++) {
        QnSystemHealth::MessageType messageType = static_cast<QnSystemHealth::MessageType>(i);
        if (!QnSystemHealth::isMessageVisible(messageType))
            continue;

        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(QnSystemHealthStringsHelper::messageTitle(messageType));
        ui->systemHealthLayout->addWidget(checkbox);
        m_systemHealthCheckBoxes[messageType] = checkbox;
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
    if (m_updating)
        return;
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    bool all = true;

    quint64 healthShown = qnSettings->popupSystemHealth();
    quint64 healthFlag = 1;

    for (int i = 0; i < QnSystemHealth::Count; i++) {
        QnSystemHealth::MessageType messageType = static_cast<QnSystemHealth::MessageType>(i);
        if (!QnSystemHealth::isMessageVisible(messageType))
            continue;

        bool checked = ((healthShown & healthFlag) == healthFlag);
        m_systemHealthCheckBoxes[messageType]->setChecked(checked);
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
    Q_ASSERT_X(!m_updating, Q_FUNC_INFO, "Should never get here while updating");
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

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
    for (int i = 0; i < QnSystemHealth::Count; i++) {
        QnSystemHealth::MessageType messageType = static_cast<QnSystemHealth::MessageType>(i);
        if (!QnSystemHealth::isMessageVisible(messageType))
            continue;

        if (m_systemHealthCheckBoxes[messageType]->isChecked() || ui->showAllCheckBox->isChecked()) {
            result |= healthFlag;
        } else {
            result &= ~healthFlag;
        }
        healthFlag = healthFlag << 1;
    }

    return result;
}
