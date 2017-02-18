/**********************************************************
* Oct 5, 2015
* akolesnikov
***********************************************************/

#pragma once

namespace nx {
namespace cdb {

extern const char* kAccountRegisterPath;
extern const char* kAccountActivatePath;
extern const char* kAccountGetPath;
extern const char* kAccountUpdatePath;
extern const char* kAccountPasswordResetPath;
extern const char* kAccountReactivatePath;
extern const char* kAccountCreateTemporaryCredentialsPath;

extern const char* kSystemBindPath;
extern const char* kSystemUnbindPath;
extern const char* kSystemGetPath;
extern const char* kSystemSharePath;
extern const char* kSystemGetCloudUsersPath;
extern const char* kSystemGetAccessRoleListPath;
extern const char* kSystemRenamePath;
extern const char* kSystemUpdatePath;
extern const char* kSystemRecordUserSessionStartPath;

extern const char* kAuthGetNoncePath;
extern const char* kAuthGetAuthenticationPath;

extern const char* kSubscribeToSystemEventsPath;

extern const char* kPingPath;

extern const char* kDeprecatedEstablishEc2TransactionConnectionPath;
extern const char* kDeprecatedPushEc2TransactionPath;

extern const char* kEstablishEc2TransactionConnectionPath;
extern const char* kPushEc2TransactionPath;

/** Maintenance. */
extern const char* kMaintenanceGetVmsConnections;
extern const char* kMaintenanceGetTransactionLog;

} // namespace cdb
} // namespace nx
