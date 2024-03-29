// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "popup_settings_widget.h"
#include "ui_popup_settings_widget.h"

#include <QtCore/QScopedValueRollback>

#include <api/server_rest_connection.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_properties.h>
#include <health/system_health_strings_helper.h>
#include <nx/reflect/json.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/settings/message_bar_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/strings_helper.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::vms::client::desktop;

using EventType = nx::vms::api::EventType;

namespace {

QList<EventType> supportedEventTypes(nx::vms::common::SystemContext* systemContext)
{
    using namespace nx::vms::event;
    return allEvents({
        isNonDeprecatedEvent,
        isApplicableForLicensingMode(systemContext)});
}

std::set<nx::vms::common::system_health::MessageType> supportedMessageTypes(
    nx::vms::common::SystemContext* systemContext)
{
    using namespace nx::vms::common::system_health;
    return allMessageTypes({
        isMessageVisibleInSettings,
        isMessageApplicableForLicensingMode(systemContext)});
}

} // namespace

QnPopupSettingsWidget::QnPopupSettingsWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::PopupSettingsWidget),
    m_businessRulesCheckBoxes(),
    m_systemHealthCheckBoxes(),
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

    for (const auto eventType: supportedEventTypes(systemContext()))
    {
        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(m_helper->eventName(eventType));
        ui->businessEventsLayout->addWidget(checkbox);
        m_businessRulesCheckBoxes[eventType] = checkbox;

        connect(checkbox, &QCheckBox::toggled, this,
            &QnAbstractPreferencesWidget::hasChangesChanged);
    }

    for (auto messageType: supportedMessageTypes(systemContext()))
    {
        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(QnSystemHealthStringsHelper::messageTitle(messageType));
        ui->systemHealthLayout->addWidget(checkbox);
        m_systemHealthCheckBoxes[messageType] = checkbox;

        connect(checkbox, &QCheckBox::toggled, this,
            &QnAbstractPreferencesWidget::hasChangesChanged);
    }

    connect(
        context(), &QnWorkbenchContext::userChanged, this, &QnPopupSettingsWidget::at_userChanged);

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

    at_userChanged(context()->user());
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

    for (auto messageType: supportedMessageTypes(systemContext()))
    {
        const bool checked = messageTypes.contains(messageType);
        m_systemHealthCheckBoxes[messageType]->setChecked(checked);

        all &= checked;
    }

    const auto eventTypes = supportedEventTypes(systemContext());
    QList<EventType> watchedEvents = m_currentUser
        ? m_currentUser->settings().watchedEvents()
        : eventTypes;

    for (const auto eventType: eventTypes)
    {
        bool checked = watchedEvents.contains(eventType);
        m_businessRulesCheckBoxes[eventType]->setChecked(checked);
        all &= checked;
    }

    ui->showAllCheckBox->setChecked(all);

    ui->businessEventsGroupBox->setEnabled((bool) m_currentUser);
}

void QnPopupSettingsWidget::applyChanges()
{
    NX_ASSERT(!m_updating, "Should never get here while updating");
    QScopedValueRollback<bool> guard(m_updating, true);

    if (m_currentUser)
    {
        auto userSettings = m_currentUser->settings();
        const auto oldUserSettings = userSettings;
        userSettings.setWatchedEvents(watchedEvents());
        m_currentUser->setSettings(userSettings);

        nx::vms::api::ResourceWithParameters parameters;
        parameters.setFromParameter({ResourcePropertyKey::User::kUserSettings,
            QString::fromStdString(nx::reflect::json::serialize(m_currentUser->settings()))});

        auto sessionTokenHelper = FreshSessionTokenHelper::makeHelper(this,
            tr("Save user"),
            tr("Enter your account password"),
            tr("Save"),
            FreshSessionTokenHelper::ActionType::updateSettings);

        systemContext()->connectedServerApi()->patchUserParameters(
            m_currentUser->getId(),
            parameters,
            sessionTokenHelper,
            nx::utils::guarded(this,
                [this, oldUserSettings](bool success,
                    int /*handle*/,
                    rest::ErrorOrData<nx::vms::api::UserModelV3> /*errorOrData*/)
                {
                    if (success)
                        return;

                    if (m_currentUser)
                        m_currentUser->setSettings(oldUserSettings);
                }),
            thread());
    }

    appContext()->localSettings()->popupSystemHealth = storedSystemHealth();
}

bool QnPopupSettingsWidget::hasChanges() const
{
    return appContext()->localSettings()->popupSystemHealth() != storedSystemHealth()
        || (m_currentUser && m_currentUser->settings().watchedEvents() != watchedEvents());
}

QList<EventType> QnPopupSettingsWidget::watchedEvents() const
{
    const auto eventTypes = supportedEventTypes(systemContext());

    if (ui->showAllCheckBox->isChecked())
        return eventTypes;

    QList<EventType> result;
    for (const auto eventType: eventTypes)
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

    for (auto messageType: supportedMessageTypes(systemContext()))
    {
        if (m_systemHealthCheckBoxes[messageType]->isChecked() || ui->showAllCheckBox->isChecked())
            result.insert(messageType);
    }

    return result;
}

void QnPopupSettingsWidget::at_userChanged(const QnUserResourcePtr& user)
{
    if (user == m_currentUser)
        return;

    if (m_currentUser)
        m_currentUser->disconnect(this);

    m_currentUser = user;

    if (!m_currentUser)
        return;

    connect(
        m_currentUser.get(),
        &QnUserResource::userSettingsChanged,
        this,
        &QnPopupSettingsWidget::loadDataToUi);
}
