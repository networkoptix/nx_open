// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "popup_settings_widget.h"
#include "ui_popup_settings_widget.h"

#include <QtCore/QScopedValueRollback>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_properties.h>
#include <health/system_health_strings_helper.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/settings/message_bar_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/resource/property_adaptors.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/strings_helper.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::vms::client::desktop;

using EventType = nx::vms::api::EventType;

QnPopupSettingsWidget::QnPopupSettingsWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::PopupSettingsWidget),
    m_businessRulesCheckBoxes(),
    m_systemHealthCheckBoxes(),
    m_adaptor(new nx::vms::common::BusinessEventFilterResourcePropertyAdaptor(this)),
    m_updating(false),
    m_helper(new nx::vms::event::StringsHelper(systemContext()))
{
    ui->setupUi(this);

    SnappedScrollBar* scrollBar = new SnappedScrollBar(this);
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());

    setHelpTopic(this, HelpTopic::Id::SystemSettings_Notifications);

    ui->deprecationMessageBar->init(
        {.text = tr("These settings apply only to the system you are logged in."
            " They will be removed in future versions."),
        .isEnabledProperty = &messageBarSettings()->notificationsDeprecationInfo});

    for (EventType eventType: nx::vms::event::allEvents())
    {
        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(m_helper->eventName(eventType));
        ui->businessEventsLayout->addWidget(checkbox);
        m_businessRulesCheckBoxes[eventType] = checkbox;

        connect(checkbox, &QCheckBox::toggled, this,
            &QnAbstractPreferencesWidget::hasChangesChanged);
    }

    for (auto messageType: nx::vms::common::system_health::allVisibleMessageTypes())
    {
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
            // TODO: #sivanov Also update checked state. Probably tristate for 'show all' checkbox
            // would be better.
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
    QScopedValueRollback<bool> guard(m_updating, true);

    bool all = true;

    std::set<nx::vms::common::system_health::MessageType> messageTypes =
        appContext()->localSettings()->popupSystemHealth();

    for (auto messageType: nx::vms::common::system_health::allVisibleMessageTypes())
    {
        const bool checked = messageTypes.contains(messageType);
        m_systemHealthCheckBoxes[messageType]->setChecked(checked);

        all &= checked;
    }

    if (context()->user())
        m_adaptor->setResource(context()->user());

    QList<EventType> watchedEvents = context()->user()
        ? m_adaptor->watchedEvents()
        : nx::vms::event::allEvents();

    for (EventType eventType: nx::vms::event::allEvents())
    {
        bool checked = watchedEvents.contains(eventType);
        m_businessRulesCheckBoxes[eventType]->setChecked(checked);
        all &= checked;
    }

    ui->showAllCheckBox->setChecked(all);

    ui->businessEventsGroupBox->setEnabled((bool) context()->user());
}

void QnPopupSettingsWidget::applyChanges()
{
    NX_ASSERT(!m_updating, "Should never get here while updating");
    QScopedValueRollback<bool> guard(m_updating, true);

    if (context()->user())
    {
        m_adaptor->setWatchedEvents(watchedEvents());
        m_adaptor->saveToResource();
        resourcePropertyDictionary()->saveParamsAsync(context()->user()->getId());
    }

    appContext()->localSettings()->popupSystemHealth = storedSystemHealth();
}

bool QnPopupSettingsWidget::hasChanges() const
{
    return appContext()->localSettings()->popupSystemHealth() != storedSystemHealth()
        || (context()->user() && m_adaptor->watchedEvents() != watchedEvents());
}

QList<EventType> QnPopupSettingsWidget::watchedEvents() const
{
    if (ui->showAllCheckBox->isChecked())
        return nx::vms::event::allEvents();

    QList<EventType> result;
    for (EventType eventType: nx::vms::event::allEvents())
    {
        if (m_businessRulesCheckBoxes[eventType]->isChecked())
            result << eventType;
    }
    return result;
}

std::set<nx::vms::common::system_health::MessageType>
    QnPopupSettingsWidget::storedSystemHealth() const
{
    std::set<nx::vms::common::system_health::MessageType> result;

    for (auto messageType: nx::vms::common::system_health::allVisibleMessageTypes())
    {
        if (nx::vms::common::system_health::isMessageVisibleInSettings(messageType)
            && (m_systemHealthCheckBoxes[messageType]->isChecked()
                || ui->showAllCheckBox->isChecked()))
        {
            result.insert(messageType);
        }
    }

    return result;
}
