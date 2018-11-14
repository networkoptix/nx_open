#include "notification.h"

#include <nx/fusion/model_functions.h>

namespace nx::cloud::db {

bool operator==(const BasicNotification& one, const BasicNotification& two)
{
    return one.user_email == two.user_email
        && one.type == two.type
        && one.customization == two.customization;
}

//-------------------------------------------------------------------------------------------------
// ActivateAccountNotification

ActivateAccountNotification::ActivateAccountNotification()
{
    type = NotificationType::activateAccount;
}

//-------------------------------------------------------------------------------------------------
// RestorePasswordNotification

RestorePasswordNotification::RestorePasswordNotification()
{
    type = NotificationType::restorePassword;
}

//-------------------------------------------------------------------------------------------------
// SystemSharedNotification

SystemSharedNotification::SystemSharedNotification()
{
    type = NotificationType::systemShared;
}

//-------------------------------------------------------------------------------------------------
// InviteUserNotification

InviteUserNotification::InviteUserNotification()
{
    type = NotificationType::systemInvite;
}

//-------------------------------------------------------------------------------------------------

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (BasicNotification) \
        (ActivateAccountData) \
        (SystemSharedData) \
        (InviteUserData) \
        (ActivateAccountNotificationData) \
        (InviteUserNotificationData) \
        (SystemSharedNotificationData),
    (json),
    _Fields)

} // namespace nx::cloud::db

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cloud::db, NotificationType,
    (nx::cloud::db::NotificationType::activateAccount, "activate_account")
    (nx::cloud::db::NotificationType::restorePassword, "restore_password")
    (nx::cloud::db::NotificationType::systemShared, "system_shared")
    (nx::cloud::db::NotificationType::systemInvite, "system_invite")
)
