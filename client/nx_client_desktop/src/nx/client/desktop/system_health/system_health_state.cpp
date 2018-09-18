#include "system_health_state.h"

#include <array>

#include <api/global_settings.h>
#include <api/common_message_processor.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_user_email_watcher.h>
#include <utils/email/email.h>

#include <nx/client/desktop/system_health/default_password_cameras_watcher.h>
#include <nx/client/desktop/system_health/system_internet_access_watcher.h>

namespace nx::client::desktop {

// ------------------------------------------------------------------------------------------------
// SystemHealthState::Private

class SystemHealthState::Private
{
    SystemHealthState* const q;

public:
    Private(SystemHealthState* q);

    bool value(SystemHealthIndex index) const;
    QVariant data(SystemHealthIndex index) const;

private:
    void setValue(SystemHealthIndex index, bool value);
    bool calculate(SystemHealthIndex index) const;

    void update(SystemHealthIndex index) { setValue(index, calculate(index)); }

    SystemInternetAccessWatcher* internetAccessWatcher() const;
    DefaultPasswordCamerasWatcher* defaultPasswordWatcher() const;
    QnWorkbenchUserEmailWatcher* userEmailWatcher() const;
    bool isAdmin() const;

private:
    std::array<bool, SystemHealthIndex::Count> m_state;
};

SystemHealthState::Private::Private(SystemHealthState* q) :
    q(q),
    m_state()
{
#define update(index) [this]() { update(SystemHealthIndex::index); }

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
    connect(messageProcessor, &QnCommonMessageProcessor::initialResourcesReceived,
        q, update(NoInternetForTimeSync));

    connect(messageProcessor, &QnCommonMessageProcessor::connectionClosed,
        q, update(NoInternetForTimeSync));

    update(NoInternetForTimeSync)();

    // DefaultCameraPasswords.

    NX_ASSERT(defaultPasswordWatcher(), "Default password cameras watcher is not initialized");

    connect(defaultPasswordWatcher(), &DefaultPasswordCamerasWatcher::cameraSetChanged,
        q, update(DefaultCameraPasswords));

    connect(q->context(), &QnWorkbenchContext::userChanged, q, update(DefaultCameraPasswords));
    update(DefaultCameraPasswords)();

    // EmailIsEmpty.

    NX_ASSERT(userEmailWatcher(), "User email watcher is not initialized");

    connect(userEmailWatcher(), &QnWorkbenchUserEmailWatcher::userEmailValidityChanged, q,
        [this](const QnUserResourcePtr& user, bool emailIsValid)
        {
            if (user && user == this->q->context()->user())
            {
                setValue(SystemHealthIndex::EmailIsEmpty, !emailIsValid
                    && this->q->accessController()->hasPermissions(user, Qn::WriteEmailPermission));
            }
        });

    connect(q->context(), &QnWorkbenchContext::userChanged, q,
        [this]() { userEmailWatcher()->forceCheck(this->q->context()->user()); });

    if (q->context()->user())
        userEmailWatcher()->forceCheck(q->context()->user());

    // TODO: #vkutin Handle all other system health state (not one-time notifications).
    // UsersEmailIsEmpty.
    // CloudPromo.

#undef update
}

bool SystemHealthState::Private::value(SystemHealthIndex index) const
{
    return m_state[index];
}

void SystemHealthState::Private::setValue(SystemHealthIndex index, bool value)
{
    if (this->value(index) == value)
        return;

    m_state[index] = value;
    emit q->changed(index, value, {});
}

bool SystemHealthState::Private::calculate(SystemHealthIndex index) const
{
    if (q->commonModule()->remoteGUID().isNull())
        return false;

    switch (index)
    {
        case SystemHealthIndex::NoInternetForTimeSync:
            return isAdmin()
                && q->globalSettings()->isSynchronizingTimeWithInternet()
                && !internetAccessWatcher()->systemHasInternetAccess();

        case SystemHealthIndex::SystemIsReadOnly:
            return isAdmin() && q->commonModule()->isReadOnly();

        case SystemHealthIndex::NoLicenses:
            return isAdmin() && q->licensePool()->isEmpty();

        case SystemHealthIndex::SmtpIsNotSet:
            return isAdmin() && !q->globalSettings()->emailSettings().isValid();

        case SystemHealthIndex::DefaultCameraPasswords:
            return isAdmin() && !defaultPasswordWatcher()->camerasWithDefaultPassword().empty();

        case SystemHealthIndex::EmailIsEmpty: // TODO: Refactor QnWorkbenchUserEmailWatcher.
            NX_ASSERT(false, "This value is not calculatable.");

        // TODO: #vkutin Handle all other system health state (not one-time notifications).
        //
        //case SystemHealthIndex::UsersEmailIsEmpty:
        //case SystemHealthIndex::CloudPromo:

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
            return QVariant::fromValue(QnVirtualCameraResourceList(
                defaultPasswordWatcher()->camerasWithDefaultPassword().values()));

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

QnWorkbenchUserEmailWatcher* SystemHealthState::Private::userEmailWatcher() const
{
    return q->context()->instance<QnWorkbenchUserEmailWatcher>();
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

bool SystemHealthState::value(SystemHealthIndex index) const
{
    return d->value(index);
}

QVariant SystemHealthState::data(SystemHealthIndex index) const
{
    return d->data(index);
}

} // namespace nx::client::desktop
