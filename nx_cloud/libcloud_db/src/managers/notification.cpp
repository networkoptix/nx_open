#include "notification.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace cdb {

//-------------------------------------------------------------------------------------------------
// ActivateAccountNotification

constexpr const char* kConfirmaionEmailNotificationName = "activate_account";

ActivateAccountNotification::ActivateAccountNotification()
{
    type = kConfirmaionEmailNotificationName;
}

//-------------------------------------------------------------------------------------------------
// RestorePasswordNotification

constexpr const char* kRestorePasswordNotificationName = "restore_password";

RestorePasswordNotification::RestorePasswordNotification()
{
    type = kRestorePasswordNotificationName;
}

//-------------------------------------------------------------------------------------------------
// SystemSharedNotification

constexpr const char* kSystemSharedNotificationName = "system_shared";

SystemSharedNotification::SystemSharedNotification()
{
    type = kSystemSharedNotificationName;
}

//-------------------------------------------------------------------------------------------------
// InviteUserNotification

constexpr const char* kInviteUserNotificationName = "system_invite";

InviteUserNotification::InviteUserNotification()
{
    type = kInviteUserNotificationName;
}

//-------------------------------------------------------------------------------------------------

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
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
