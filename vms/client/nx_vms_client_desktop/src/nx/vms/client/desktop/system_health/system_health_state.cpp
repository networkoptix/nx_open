#include "system_health_state.h"

#include <array>

#include <QtCore/QSet>
#include <QtWidgets/QAction>

#include <api/global_settings.h>
#include <api/common_message_processor.h>
#include <api/runtime_info_manager.h>
#include <client/client_show_once_settings.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <utils/email/email.h>

#include <nx/vms/client/desktop/system_health/default_password_cameras_watcher.h>
#include <nx/vms/client/desktop/system_health/system_internet_access_watcher.h>
#include <nx/vms/client/desktop/system_health/user_emails_watcher.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>

namespace nx::vms::client::desktop {

namespace {

static const QString kCloudPromoShowOnceKey(lit("CloudPromoNotification"));

} // namespace

// ------------------------------------------------------------------------------------------------
// SystemHealthState::Private

class SystemHealthState::Private
{
    SystemHealthState* const q;

public:
    Private(SystemHealthState* q);

    bool state(SystemHealthIndex index) const;
    QVariant data(SystemHealthIndex index) const;

private:
    bool setState(SystemHealthIndex index, bool value);
    bool calculateState(SystemHealthIndex index) const;

    bool update(SystemHealthIndex index) { return setState(index, calculateState(index)); }

    SystemInternetAccessWatcher* internetAccessWatcher() const;
    DefaultPasswordCamerasWatcher* defaultPasswordWatcher() const;
    UserEmailsWatcher* userEmailsWatcher() const;
    bool isAdmin() const;

    void updateCamerasWithDefaultPassword();
    void updateUsersWithInvalidEmail();
    void updateServersWithoutStorages();

private:
    std::array<bool, SystemHealthIndex::Count> m_state;
    QnUserResourceList m_usersWithInvalidEmail; //< Only editable; current user excluded.
    QnVirtualCameraResourceList m_camerasWithDefaultPassword;
    QnMediaServerResourceList m_serversWithoutStorages;
};

SystemHealthState::Private::Private(SystemHealthState* q) :
    q(q),
    m_state()
{
#define update(index) [this]() { return update(SystemHealthIndex::index); }

    // NoLicenses.

    connect(q->licensePool(), &QnLicensePool::licensesChanged, q, update(NoLicenses));
    connect(q->context(), &QnWorkbenchContext::userChanged, q, update(NoLicenses));

    update(NoLicenses)();

    // SystemIsReadOnly.

    connect(q->commonModule(), &QnCommonModule::readOnlyChanged, q, update(SystemIsReadOnly));
    connect(q->context(), &QnWorkbenchContext::userChanged, q, update(SystemIsReadOnly));

    update(SystemIsReadOnly)();

    // SmtpIsNotSet.

    connect(q->globalSettings(), &QnGlobalSettings::emailSettingsChanged, q, update(SmtpIsNotSet));
    connect(q->context(), &QnWorkbenchContext::userChanged, q, update(SmtpIsNotSet));

    update(SmtpIsNotSet)();

    // NoInternetForTimeSync.

    NX_ASSERT(internetAccessWatcher(), "Internet access watcher is not initialized");

    connect(internetAccessWatcher(), &SystemInternetAccessWatcher::internetAccessChanged,
        q, update(NoInternetForTimeSync));

    connect(q->globalSettings(), &QnGlobalSettings::timeSynchronizationSettingsChanged,
        q, update(NoInternetForTimeSync));

    const auto messageProcessor = q->commonModule()->messageProcessor();
    connect(messageProcessor, &QnCommonMessageProcessor::initialResourcesReceived, q,
        [this]()
        {
            update(NoInternetForTimeSync);
            updateServersWithoutStorages();
        });

    connect(messageProcessor, &QnCommonMessageProcessor::connectionClosed,
        q, update(NoInternetForTimeSync));

    update(NoInternetForTimeSync)();

    // DefaultCameraPasswords.

    NX_ASSERT(defaultPasswordWatcher(), "Default password cameras watcher is not initialized");

    connect(defaultPasswordWatcher(), &DefaultPasswordCamerasWatcher::cameraSetChanged, q,
        [this]() { updateCamerasWithDefaultPassword(); });

    connect(q->context(), &QnWorkbenchContext::userChanged, q,
        [this]() { updateCamerasWithDefaultPassword(); });

    updateCamerasWithDefaultPassword();

    // EmailIsEmpty.

    NX_ASSERT(userEmailsWatcher(), "User email watcher is not initialized");

    connect(userEmailsWatcher(), &UserEmailsWatcher::userSetChanged, q, update(EmailIsEmpty));
    connect(q->context(), &QnWorkbenchContext::userChanged, q, update(EmailIsEmpty));
    update(EmailIsEmpty)();

    // UsersEmailIsEmpty.

    connect(userEmailsWatcher(), &UserEmailsWatcher::userSetChanged, q,
        [this]() { updateUsersWithInvalidEmail(); });

    connect(q->context(), &QnWorkbenchContext::userChanged, q,
        [this]() { updateUsersWithInvalidEmail(); });

    updateUsersWithInvalidEmail();

    // StoragesNotConfigured.

    const auto runtimeInfoManager = q->context()->runtimeInfoManager();
    NX_ASSERT(runtimeInfoManager);

    connect(runtimeInfoManager, &QnRuntimeInfoManager::runtimeInfoAdded, q,
        [this]() { updateServersWithoutStorages(); });

    connect(runtimeInfoManager, &QnRuntimeInfoManager::runtimeInfoRemoved, q,
        [this]() { updateServersWithoutStorages(); });

    connect(runtimeInfoManager, &QnRuntimeInfoManager::runtimeInfoChanged, q,
        [this]() { updateServersWithoutStorages(); });

    updateServersWithoutStorages();

    // CloudPromo.

    connect(q->action(ui::action::HideCloudPromoAction), &QAction::triggered, q,
        [this]
        {
            qnClientShowOnce->setFlag(kCloudPromoShowOnceKey);
            update(CloudPromo)();
        });

    connect(q->context(), &QnWorkbenchContext::userChanged, q, update(CloudPromo));
    update(CloudPromo)();

#undef update
}

void SystemHealthState::Private::updateCamerasWithDefaultPassword()
{
    // Cache cameras with default password as a list.
    m_camerasWithDefaultPassword =
        defaultPasswordWatcher()->camerasWithDefaultPassword().toList();

    if (!update(SystemHealthIndex::DefaultCameraPasswords))
        emit q->dataChanged(SystemHealthIndex::DefaultCameraPasswords, {});
}

void SystemHealthState::Private::updateUsersWithInvalidEmail()
{
    // Cache filtered users with invalid emails as a list.
    const auto usersWithInvalidEmail = QnUserResourceList(
        userEmailsWatcher()->usersWithInvalidEmail().toList()).filtered(
            [this](const QnUserResourcePtr& user)
            {
                return user && user != q->context()->user()
                    && q->accessController()->hasPermissions(user,
                        Qn::WriteEmailPermission);
            });

    if (m_usersWithInvalidEmail == usersWithInvalidEmail)
        return;

    m_usersWithInvalidEmail = usersWithInvalidEmail;

    if (!update(SystemHealthIndex::UsersEmailIsEmpty))
        emit q->dataChanged(SystemHealthIndex::UsersEmailIsEmpty, {});
}

void SystemHealthState::Private::updateServersWithoutStorages()
{
    const auto calculateServersWithoutStorages =
        [this]() -> QnMediaServerResourceList
        {
            const auto runtimeInfoManager = q->context()->runtimeInfoManager();
            if (!NX_ASSERT(runtimeInfoManager))
                return {};

            QSet<QnUuid> serversWithoutStorages;

            const auto items = runtimeInfoManager->items()->getItems();
            for (const auto item: items)
            {
                if (item.data.flags.testFlag(nx::vms::api::RuntimeFlag::noStorages))
                    serversWithoutStorages.insert(item.uuid);
            }

            return q->commonModule()->resourcePool()->getResourcesByIds<QnMediaServerResource>(
                serversWithoutStorages);
        };

    const auto serversWithoutStorages = calculateServersWithoutStorages();
    if (serversWithoutStorages == m_serversWithoutStorages)
        return;

    m_serversWithoutStorages = serversWithoutStorages;

    if (!update(SystemHealthIndex::StoragesNotConfigured))
        emit q->dataChanged(SystemHealthIndex::StoragesNotConfigured, {});
}

bool SystemHealthState::Private::state(SystemHealthIndex index) const
{
    return m_state[index];
}

bool SystemHealthState::Private::setState(SystemHealthIndex index, bool value)
{
    if (state(index) == value)
        return false;

    m_state[index] = value;
    emit q->stateChanged(index, value, {});

    return true;
}

bool SystemHealthState::Private::calculateState(SystemHealthIndex index) const
{
    if (q->commonModule()->remoteGUID().isNull())
        return false;

    switch (index)
    {
        case SystemHealthIndex::NoInternetForTimeSync:
            return isAdmin()
                && q->globalSettings()->isTimeSynchronizationEnabled()
                && q->globalSettings()->primaryTimeServer().isNull()
                && !internetAccessWatcher()->systemHasInternetAccess();

        case SystemHealthIndex::SystemIsReadOnly:
            return isAdmin() && q->commonModule()->isReadOnly();

        case SystemHealthIndex::NoLicenses:
            return isAdmin() && q->licensePool()->isEmpty();

        case SystemHealthIndex::SmtpIsNotSet:
            return isAdmin() && !q->globalSettings()->emailSettings().isValid();

        case SystemHealthIndex::DefaultCameraPasswords:
            return isAdmin() && !m_camerasWithDefaultPassword.empty();

        case SystemHealthIndex::EmailIsEmpty:
        {
            const auto user = q->context()->user();
            return user && userEmailsWatcher()->usersWithInvalidEmail().contains(user)
                && q->accessController()->hasPermissions(user, Qn::WriteEmailPermission);
        }

        case SystemHealthIndex::UsersEmailIsEmpty:
            return !m_usersWithInvalidEmail.empty();

        case SystemHealthIndex::CloudPromo:
            return q->context()->user()
                && q->context()->user()->userRole() == Qn::UserRole::owner
                && q->globalSettings()->cloudSystemId().isNull()
                && !qnClientShowOnce->testFlag(kCloudPromoShowOnceKey);

        case SystemHealthIndex::StoragesNotConfigured:
            return isAdmin() && !m_serversWithoutStorages.empty();

        default:
            NX_ASSERT(false, "This system health index is not handled by SystemHealthState");
            return false;
    }
}

QVariant SystemHealthState::Private::data(SystemHealthIndex index) const
{
    if (!m_state[index])
        return {};

    switch (index)
    {
        case SystemHealthIndex::DefaultCameraPasswords:
            return QVariant::fromValue(m_camerasWithDefaultPassword);

        case SystemHealthIndex::UsersEmailIsEmpty:
            return QVariant::fromValue(m_usersWithInvalidEmail);

        case SystemHealthIndex::StoragesNotConfigured:
            return QVariant::fromValue(m_serversWithoutStorages);

        default:
            return {};
    }
}

bool SystemHealthState::Private::isAdmin() const
{
    return q->accessController()->hasGlobalPermission(GlobalPermission::admin);
}

SystemInternetAccessWatcher* SystemHealthState::Private::internetAccessWatcher() const
{
    return q->commonModule()->instance<SystemInternetAccessWatcher>();
}

DefaultPasswordCamerasWatcher* SystemHealthState::Private::defaultPasswordWatcher() const
{
    return q->context()->instance<DefaultPasswordCamerasWatcher>();
}

UserEmailsWatcher* SystemHealthState::Private::userEmailsWatcher() const
{
    return q->context()->instance<UserEmailsWatcher>();
}

// ------------------------------------------------------------------------------------------------
// SystemHealthState

SystemHealthState::SystemHealthState(QObject* parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(this))
{
}

SystemHealthState::~SystemHealthState()
{
}

bool SystemHealthState::state(SystemHealthIndex index) const
{
    return d->state(index);
}

QVariant SystemHealthState::data(SystemHealthIndex index) const
{
    return d->data(index);
}

} // namespace nx::vms::client::desktop
