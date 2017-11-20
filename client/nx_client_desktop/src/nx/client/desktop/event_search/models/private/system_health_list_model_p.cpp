#include "system_health_list_model_p.h"

#include <chrono>

#include <client/client_settings.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <health/system_health_helper.h>
#include <ui/common/notification_levels.h>
#include <ui/dialogs/resource_properties/user_settings_dialog.h>
#include <ui/dialogs/resource_properties/server_settings_dialog.h>
#include <ui/help/business_help.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/handlers/workbench_notifications_handler.h>

#include <nx/client/desktop/ui/actions/action.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/strings_helper.h>

namespace nx {
namespace client {
namespace desktop {

using namespace ui;

//-------------------------------------------------------------------------------------------------
// SystemHealthListModel::Private::Item

SystemHealthListModel::Private::Item::Item(
    QnSystemHealth::MessageType message,
    const QnResourcePtr& resource)
    :
    message(message),
    resource(resource)
{
}

SystemHealthListModel::Private::Item::operator QnSystemHealth::MessageType() const
{
    return message;
}

bool SystemHealthListModel::Private::Item::operator==(const Item& other) const
{
    return message == other.message && resource == other.resource;
}

bool SystemHealthListModel::Private::Item::operator!=(const Item& other) const
{
    return !(*this == other);
}

bool SystemHealthListModel::Private::Item::operator<(const Item& other) const
{
    return message != other.message
        ? message < other.message
        : resource < other.resource;
}

//-------------------------------------------------------------------------------------------------
// SystemHealthListModel::Private

SystemHealthListModel::Private::Private(SystemHealthListModel* q) :
    base_type(),
    QnWorkbenchContextAware(q),
    q(q),
    m_helper(new vms::event::StringsHelper(commonModule()))
{
    const auto handler = context()->instance<QnWorkbenchNotificationsHandler>();
    connect(handler, &QnWorkbenchNotificationsHandler::cleared, this, &Private::clear);
    connect(handler, &QnWorkbenchNotificationsHandler::systemHealthEventAdded,
        this, &Private::addSystemHealthEvent);
    connect(handler, &QnWorkbenchNotificationsHandler::systemHealthEventRemoved,
        this, &Private::removeSystemHealthEvent);
}

SystemHealthListModel::Private::~Private()
{
}

int SystemHealthListModel::Private::count() const
{
    return int(m_items.size());
}

QnSystemHealth::MessageType SystemHealthListModel::Private::message(int index) const
{
    return m_items[index].message;
}

QnResourcePtr SystemHealthListModel::Private::resource(int index) const
{
    return m_items[index].resource;
}

QString SystemHealthListModel::Private::text(int index) const
{
    const auto& item = m_items[index];
    return QnSystemHealthStringsHelper::messageText(item.message,
        QnResourceDisplayInfo(item.resource).toString(qnSettings->extraInfoInTree()));
}

QString SystemHealthListModel::Private::toolTip(int index) const
{
    const auto& item = m_items[index];
    return QnSystemHealthStringsHelper::messageTooltip(item.message,
        QnResourceDisplayInfo(item.resource).toString(qnSettings->extraInfoInTree()));
}

QPixmap SystemHealthListModel::Private::pixmap(int index) const
{
    return pixmap(m_items[index].message);
}

QColor SystemHealthListModel::Private::color(int index) const
{
    return QnNotificationLevel::notificationTextColor(
        QnNotificationLevel::valueOf(m_items[index].message));
}

int SystemHealthListModel::Private::helpId(int index) const
{
    return QnBusiness::healthHelpId(m_items[index].message);
}

int SystemHealthListModel::Private::priority(int index) const
{
    return priority(m_items[index].message);
}

bool SystemHealthListModel::Private::locked(int index) const
{
    return QnSystemHealth::isMessageLocked(m_items[index].message);
}

action::IDType SystemHealthListModel::Private::action(int index) const
{
    switch (m_items[index].message)
    {
        case QnSystemHealth::EmailIsEmpty:
            return action::UserSettingsAction;

        case QnSystemHealth::NoLicenses:
            return action::PreferencesLicensesTabAction;

        case QnSystemHealth::SmtpIsNotSet:
            return action::PreferencesSmtpTabAction;

        case QnSystemHealth::UsersEmailIsEmpty:
            return action::UserSettingsAction;

        case QnSystemHealth::EmailSendError:
            return action::PreferencesSmtpTabAction;
            break;

        case QnSystemHealth::StoragesNotConfigured:
        case QnSystemHealth::StoragesAreFull:
        case QnSystemHealth::ArchiveRebuildFinished:
        case QnSystemHealth::ArchiveRebuildCanceled:
            return action::ServerSettingsAction;

        case QnSystemHealth::CloudPromo:
            return action::PreferencesCloudTabAction;

        default:
            return action::NoAction;
    }
}

action::Parameters SystemHealthListModel::Private::parameters(int index) const
{
    switch (m_items[index].message)
    {
        case QnSystemHealth::EmailIsEmpty:
            return action::Parameters(context()->user())
                .withArgument(Qn::FocusElementRole, lit("email"))
                .withArgument(Qn::FocusTabRole, QnUserSettingsDialog::SettingsPage);

        case QnSystemHealth::UsersEmailIsEmpty:
            return action::Parameters(m_items[index].resource)
                .withArgument(Qn::FocusElementRole, lit("email"))
                .withArgument(Qn::FocusTabRole, QnUserSettingsDialog::SettingsPage);

        case QnSystemHealth::StoragesNotConfigured:
        case QnSystemHealth::StoragesAreFull:
        case QnSystemHealth::ArchiveRebuildFinished:
        case QnSystemHealth::ArchiveRebuildCanceled:
            return action::Parameters(m_items[index].resource)
                .withArgument(Qn::FocusTabRole, QnServerSettingsDialog::StorageManagmentPage);

        default:
            return action::Parameters();
    }
}

void SystemHealthListModel::Private::addSystemHealthEvent(
    QnSystemHealth::MessageType message,
    const QVariant& params)
{
    QnResourcePtr resource;
    if (params.canConvert<QnResourcePtr>())
        resource = params.value<QnResourcePtr>();

    vms::event::AbstractActionPtr action;
    if (params.canConvert<vms::event::AbstractActionPtr>())
    {
        action = params.value<vms::event::AbstractActionPtr>();
        if (action)
        {
            auto resourceId = action->getRuntimeParams().eventResourceId;
            resource = resourcePool()->getResourceById(resourceId);
        }
    }

    // TODO: #vkutin We may want multiple resource aggregation as one event.
    Item item(message, resource);
    auto position = std::lower_bound(m_items.begin(), m_items.end(), item);
    if (position != m_items.end() && *position == item)
        return; //< Already exists.

    const auto index = std::distance(m_items.begin(), position);

    ScopedInsertRows insertRows(q, QModelIndex(), index, index);
    m_items.insert(position, item);
}

void SystemHealthListModel::Private::removeSystemHealthEvent(
    QnSystemHealth::MessageType message,
    const QVariant& params)
{
    QnResourcePtr resource;
    if (params.canConvert<QnResourcePtr>())
        resource = params.value<QnResourcePtr>();

    if (resource)
    {
        const Item item(message, resource);
        const auto position = std::lower_bound(m_items.cbegin(), m_items.cend(), item);
        if (position != m_items.cend() && *position == item)
            q->removeRows(std::distance(m_items.cbegin(), position), 1);
    }
    else
    {
        const auto range = std::equal_range(m_items.cbegin(), m_items.cend(), message);
        const auto count = std::distance(range.first, range.second);
        if (count > 0)
            q->removeRows(std::distance(m_items.cbegin(), range.first), count);
    }
}

void SystemHealthListModel::Private::clear()
{
    if (m_items.empty())
        return;

    ScopedReset reset(q);
    m_items.clear();
}

void SystemHealthListModel::Private::remove(int first, int count)
{
    ScopedRemoveRows removeRows(q, QModelIndex(), first, first + count - 1);
    m_items.erase(m_items.begin() + first, m_items.begin() + (first + count));
}

int SystemHealthListModel::Private::priority(QnSystemHealth::MessageType message)
{
    switch (message)
    {
        case QnSystemHealth::CloudPromo:
            return int(QnNotificationLevel::Value::LevelCount); //< The highest.

        default:
            return int(QnNotificationLevel::valueOf(message));
    }
}

QPixmap SystemHealthListModel::Private::pixmap(QnSystemHealth::MessageType message)
{
    switch (message)
    {
        case QnSystemHealth::EmailIsEmpty:
            return qnSkin->pixmap("events/email.png");

        case QnSystemHealth::NoLicenses:
            return qnSkin->pixmap("events/license.png");

        case QnSystemHealth::SmtpIsNotSet:
            return qnSkin->pixmap("events/email.png");

        case QnSystemHealth::UsersEmailIsEmpty:
            return qnSkin->pixmap("events/email.png");

        case QnSystemHealth::SystemIsReadOnly:
            return qnSkin->pixmap("tree/system.png");

        case QnSystemHealth::EmailSendError:
            return qnSkin->pixmap("events/email.png");

        case QnSystemHealth::StoragesNotConfigured:
        case QnSystemHealth::StoragesAreFull:
        case QnSystemHealth::ArchiveRebuildFinished:
        case QnSystemHealth::ArchiveRebuildCanceled:
            return qnSkin->pixmap("events/storage.png");

        case QnSystemHealth::CloudPromo:
            return qnSkin->pixmap("cloud/cloud_20.png");

        default:
            return QPixmap();
    }
}

} // namespace
} // namespace client
} // namespace nx
