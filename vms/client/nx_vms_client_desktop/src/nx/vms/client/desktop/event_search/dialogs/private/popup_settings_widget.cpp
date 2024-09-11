// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "popup_settings_widget.h"
#include "ui_popup_settings_widget.h"

#include <QtCore/QScopedValueRollback>

#include <api/server_rest_connection.h>
#include <core/resource_management/resource_properties.h>
#include <health/system_health_strings_helper.h>
#include <nx/reflect/json.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/settings/message_bar_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>
#include <nx/vms/client/desktop/utils/site_notification_settings_manager.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/strings_helper.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

PopupSettingsWidget::PopupSettingsWidget(
    SiteNotificationSettingsManager* siteNotificationSettingsManager,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::PopupSettingsWidget),
    m_updating(false),
    m_siteNotificationSettingsManager{siteNotificationSettingsManager},
    m_helper(new nx::vms::event::StringsHelper(siteNotificationSettingsManager->systemContext()))
{
    ui->setupUi(this);

    SnappedScrollBar* scrollBar = new SnappedScrollBar(this);
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());

    setHelpTopic(this, HelpTopic::Id::SystemSettings_Notifications);

    for (const auto eventType: m_siteNotificationSettingsManager->supportedEventTypes())
    {
        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(m_helper->eventName(eventType));
        ui->businessEventsLayout->addWidget(checkbox);
        m_businessRulesCheckBoxes[eventType] = checkbox;

        connect(checkbox, &QCheckBox::toggled, this,
            &QnAbstractPreferencesWidget::hasChangesChanged);
    }

    for (auto messageType: m_siteNotificationSettingsManager->supportedMessageTypes())
    {
        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(QnSystemHealthStringsHelper::messageShortTitle(messageType));
        ui->systemHealthLayout->addWidget(checkbox);
        m_systemHealthCheckBoxes[messageType] = checkbox;

        connect(checkbox, &QCheckBox::toggled, this,
            &QnAbstractPreferencesWidget::hasChangesChanged);
    }

    connect(
        m_siteNotificationSettingsManager,
        &SiteNotificationSettingsManager::settingsChanged,
        this,
        &PopupSettingsWidget::loadDataToUi);

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

PopupSettingsWidget::~PopupSettingsWidget()
{
}

void PopupSettingsWidget::loadDataToUi()
{
    if (m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    bool all = true;

    std::set<nx::vms::common::system_health::MessageType> watchedMessageTypes =
        m_siteNotificationSettingsManager->watchedMessages();

    for (auto messageType: m_siteNotificationSettingsManager->supportedMessageTypes())
    {
        const bool checked = watchedMessageTypes.contains(messageType);
        m_systemHealthCheckBoxes[messageType]->setChecked(checked);

        all &= checked;
    }

    const auto supportedEventTypes = m_siteNotificationSettingsManager->supportedEventTypes();
    const auto watchedEvents =
        m_siteNotificationSettingsManager->watchedEvents();
    const auto selectedEvents = watchedEvents.value_or(supportedEventTypes);

    for (const auto eventType: supportedEventTypes)
    {
        bool checked = selectedEvents.contains(eventType);
        m_businessRulesCheckBoxes[eventType]->setChecked(checked);
        all &= checked;
    }

    ui->showAllCheckBox->setChecked(all);
    ui->businessEventsGroupBox->setEnabled(watchedEvents.has_value());
}

void PopupSettingsWidget::applyChanges()
{
    NX_ASSERT(!m_updating, "Should never get here while updating");
    QScopedValueRollback<bool> guard(m_updating, true);

    m_siteNotificationSettingsManager->setWatchedEvents(watchedEvents());
    m_siteNotificationSettingsManager->setWatchedMessages(storedSystemHealth());
}

bool PopupSettingsWidget::hasChanges() const
{
    if (m_siteNotificationSettingsManager->watchedMessages() != storedSystemHealth())
        return true;

    const auto userWatchedEvents = m_siteNotificationSettingsManager->watchedEvents();
    return userWatchedEvents && userWatchedEvents != watchedEvents();
}

QList<api::EventType> PopupSettingsWidget::watchedEvents() const
{
    auto eventTypes = m_siteNotificationSettingsManager->supportedEventTypes();

    if (ui->showAllCheckBox->isChecked())
        return eventTypes;

    QList<api::EventType> result;
    for (const auto eventType: eventTypes)
    {
        if (m_businessRulesCheckBoxes[eventType]->isChecked())
            result << eventType;
    }
    return result;
}

std::set<nx::vms::common::system_health::MessageType>
    PopupSettingsWidget::storedSystemHealth() const
{
    std::set<nx::vms::common::system_health::MessageType> result;

    for (auto messageType: m_siteNotificationSettingsManager->supportedMessageTypes())
    {
        if (m_systemHealthCheckBoxes[messageType]->isChecked() || ui->showAllCheckBox->isChecked())
            result.insert(messageType);
    }

    return result;
}

} // namespace nx::vms::client::desktop
