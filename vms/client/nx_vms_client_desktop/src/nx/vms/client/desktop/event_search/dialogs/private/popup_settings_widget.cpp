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
#include <nx/vms/client/desktop/utils/user_notification_settings_manager.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/strings_helper.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

PopupSettingsWidget::PopupSettingsWidget(
    UserNotificationSettingsManager* userNotificationSettingsManager,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::PopupSettingsWidget),
    m_updating(false),
    m_userNotificationSettingsManager{userNotificationSettingsManager},
    m_stringsHelper(new event::StringsHelper(userNotificationSettingsManager->systemContext()))
{
    ui->setupUi(this);

    SnappedScrollBar* scrollBar = new SnappedScrollBar(this);
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());

    setHelpTopic(this, HelpTopic::Id::SystemSettings_Notifications);

    for (auto eventType: m_userNotificationSettingsManager->supportedEventTypes())
    {
        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(m_stringsHelper->eventName(eventType));
        ui->businessEventsLayout->addWidget(checkbox);
        m_eventRulesCheckBoxes[eventType] = checkbox;

        connect(checkbox, &QCheckBox::toggled, this,
            &QnAbstractPreferencesWidget::hasChangesChanged);
    }

    for (auto messageType: m_userNotificationSettingsManager->supportedMessageTypes())
    {
        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(QnSystemHealthStringsHelper::messageShortTitle(messageType));
        ui->systemHealthLayout->addWidget(checkbox);
        m_systemHealthCheckBoxes[messageType] = checkbox;

        connect(checkbox, &QCheckBox::toggled, this,
            &QnAbstractPreferencesWidget::hasChangesChanged);
    }

    connect(
        m_userNotificationSettingsManager,
        &UserNotificationSettingsManager::settingsChanged,
        this,
        &PopupSettingsWidget::loadDataToUi);

    connect(ui->showAllCheckBox, &QCheckBox::toggled, this,
        [this](bool checked)
        {
            // TODO: #sivanov Also update checked state. Probably tristate for 'show all' checkbox
            // would be better.
            for (QCheckBox* checkbox : m_eventRulesCheckBoxes)
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

    const auto& watchedMessages = m_userNotificationSettingsManager->watchedMessages();
    for (auto messageType: m_userNotificationSettingsManager->supportedMessageTypes())
    {
        const bool checked = watchedMessages.contains(messageType);
        m_systemHealthCheckBoxes[messageType]->setChecked(checked);
        all &= checked;
    }

    const auto& watchedEvents = m_userNotificationSettingsManager->watchedEvents();
    for (const auto eventType: m_userNotificationSettingsManager->supportedEventTypes())
    {
        bool checked = watchedEvents.contains(eventType);
        m_eventRulesCheckBoxes[eventType]->setChecked(checked);
        all &= checked;
    }

    ui->showAllCheckBox->setChecked(all);
}

void PopupSettingsWidget::applyChanges()
{
    NX_ASSERT(!m_updating, "Should never get here while updating");
    QScopedValueRollback<bool> guard(m_updating, true);

    m_userNotificationSettingsManager->setSettings(watchedEvents(), watchedMessages());
}

bool PopupSettingsWidget::hasChanges() const
{
    return m_userNotificationSettingsManager->watchedMessages() != watchedMessages()
        || m_userNotificationSettingsManager->watchedEvents() != watchedEvents();
}

QList<api::EventType> PopupSettingsWidget::watchedEvents() const
{
    const auto& eventTypes = m_userNotificationSettingsManager->supportedEventTypes();
    if (ui->showAllCheckBox->isChecked())
        return eventTypes;

    QList<api::EventType> result;
    for (const auto eventType: eventTypes)
    {
        if (m_eventRulesCheckBoxes[eventType]->isChecked())
            result << eventType;
    }
    return result;
}

QList<nx::vms::common::system_health::MessageType>PopupSettingsWidget::watchedMessages() const
{
    const auto& messageTypes = m_userNotificationSettingsManager->supportedMessageTypes();
    if (ui->showAllCheckBox->isChecked())
        return messageTypes;

    QList<common::system_health::MessageType> result;
    for (auto messageType: messageTypes)
    {
        if (m_systemHealthCheckBoxes[messageType]->isChecked())
            result.push_back(messageType);
    }

    return result;
}

} // namespace nx::vms::client::desktop
