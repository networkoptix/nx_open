#include "popup_settings_widget.h"
#include "ui_popup_settings_widget.h"

#include <client/client_settings.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_properties.h>

#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/strings_helper.h>

#include <health/system_health_helper.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/workbench/workbench_context.h>

#include <utils/resource_property_adaptors.h>
#include <utils/common/scoped_value_rollback.h>

using namespace nx;

QnPopupSettingsWidget::QnPopupSettingsWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::PopupSettingsWidget),
    m_businessRulesCheckBoxes(),
    m_systemHealthCheckBoxes(),
    m_adaptor(new QnBusinessEventsFilterResourcePropertyAdaptor(this)),
    m_updating(false),
    m_helper(new vms::event::StringsHelper(commonModule()))
{
    ui->setupUi(this);

    QnSnappedScrollBar* scrollBar = new QnSnappedScrollBar(this);
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());

    setHelpTopic(this, Qn::SystemSettings_Notifications_Help);

    for (vms::event::EventType eventType : vms::event::allEvents())
    {
        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(m_helper->eventName(eventType));
        ui->businessEventsLayout->addWidget(checkbox);
        m_businessRulesCheckBoxes[eventType] = checkbox;

        connect(checkbox, &QCheckBox::toggled, this,
            &QnAbstractPreferencesWidget::hasChangesChanged);
    }

    static_assert(QnSystemHealth::Count < 64, "We are packing messages to single quint64");

    for (int i = 0; i < QnSystemHealth::Count; i++)
    {
        QnSystemHealth::MessageType messageType = static_cast<QnSystemHealth::MessageType>(i);
        if (!QnSystemHealth::isMessageVisibleInSettings(messageType))
            continue;

        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(QnSystemHealthStringsHelper::messageTitle(messageType));
        ui->systemHealthLayout->addWidget(checkbox);
        m_systemHealthCheckBoxes[messageType] = checkbox;

        connect(checkbox, &QCheckBox::toggled, this,
            &QnAbstractPreferencesWidget::hasChangesChanged);
    }

    connect(m_adaptor, &QnAbstractResourcePropertyAdaptor::valueChanged, this,
        &QnPopupSettingsWidget::loadDataToUi);

    connect(ui->showAllCheckBox, &QCheckBox::toggled, this,
        [this](bool checked)
        {
            // TODO: #GDM #Common also update checked state!
            // TODO: #GDM #Common maybe tristate for 'show all' checkbox would be better.
            for (QCheckBox* checkbox : m_businessRulesCheckBoxes)
                checkbox->setEnabled(!checked);

            for (QCheckBox* checkbox : m_systemHealthCheckBoxes)
                checkbox->setEnabled(!checked);

            emit hasChangesChanged();
        });

}

QnPopupSettingsWidget::~QnPopupSettingsWidget()
{
}

void QnPopupSettingsWidget::loadDataToUi()
{
    if (m_updating)
        return;
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    bool all = true;

    quint64 healthShown = qnSettings->popupSystemHealth();
    quint64 healthFlag = 1;

    for (int i = 0; i < QnSystemHealth::Count; i++)
    {
        QnSystemHealth::MessageType messageType = static_cast<QnSystemHealth::MessageType>(i);
        if (!QnSystemHealth::isMessageVisibleInSettings(messageType))
            continue;

        bool checked = ((healthShown & healthFlag) == healthFlag);
        m_systemHealthCheckBoxes[messageType]->setChecked(checked);
        healthFlag = healthFlag << 1;

        all &= checked;
    }

    if (context()->user())
        m_adaptor->setResource(context()->user());

    QList<vms::event::EventType> watchedEvents = context()->user()
        ? m_adaptor->watchedEvents()
        : vms::event::allEvents();

    for (vms::event::EventType eventType : m_businessRulesCheckBoxes.keys())
    {
        bool checked = watchedEvents.contains(eventType);
        m_businessRulesCheckBoxes[eventType]->setChecked(checked);
        all &= checked;
    }

    ui->showAllCheckBox->setChecked(all);

    ui->businessEventsGroupBox->setEnabled(context()->user());
}

void QnPopupSettingsWidget::applyChanges()
{
    NX_ASSERT(!m_updating, Q_FUNC_INFO, "Should never get here while updating");
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    if (context()->user())
    {
        m_adaptor->setWatchedEvents(watchedEvents());
        m_adaptor->saveToResource();
        propertyDictionary()->saveParamsAsync(context()->user()->getId());
    }

    qnSettings->setPopupSystemHealth(watchedSystemHealth());
}

bool QnPopupSettingsWidget::hasChanges() const
{
    return qnSettings->popupSystemHealth() != watchedSystemHealth()
        || (context()->user() && m_adaptor->watchedEvents() != watchedEvents());
}

QList<vms::event::EventType> QnPopupSettingsWidget::watchedEvents() const
{
    if (ui->showAllCheckBox->isChecked())
        return vms::event::allEvents();

    QList<vms::event::EventType> result;
    for (vms::event::EventType eventType : m_businessRulesCheckBoxes.keys())
        if (m_businessRulesCheckBoxes[eventType]->isChecked())
            result << eventType;
    return result;
}

quint64 QnPopupSettingsWidget::watchedSystemHealth() const
{
    quint64 result = qnSettings->popupSystemHealth();
    quint64 healthFlag = 1;
    for (int i = 0; i < QnSystemHealth::Count; i++)
    {
        QnSystemHealth::MessageType messageType = static_cast<QnSystemHealth::MessageType>(i);
        if (!QnSystemHealth::isMessageVisibleInSettings(messageType))
            continue;

        if (m_systemHealthCheckBoxes[messageType]->isChecked() || ui->showAllCheckBox->isChecked())
        {
            result |= healthFlag;
        }
        else
        {
            result &= ~healthFlag;
        }
        healthFlag = healthFlag << 1;
    }

    return result;
}
