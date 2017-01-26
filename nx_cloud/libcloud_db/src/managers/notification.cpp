#include "notification.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace cdb {

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

} // namespace cdb
} // namespace nx

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cdb, NotificationType,
    (nx::cdb::NotificationType::activateAccount, "activate_account")
    (nx::cdb::NotificationType::restorePassword, "restore_password")
    (nx::cdb::NotificationType::systemShared, "system_shared")
    (nx::cdb::NotificationType::systemInvite, "system_invite")
)
