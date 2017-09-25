#pragma once

namespace nx {
namespace cdb {

extern const char* const kAccountRegisterPath;
extern const char* const kAccountActivatePath;
extern const char* const kAccountGetPath;
extern const char* const kAccountUpdatePath;
extern const char* const kAccountPasswordResetPath;
extern const char* const kAccountReactivatePath;
extern const char* const kAccountCreateTemporaryCredentialsPath;

extern const char* const kSystemBindPath;
extern const char* const kSystemUnbindPath;
extern const char* const kSystemGetPath;
extern const char* const kSystemSharePath;
extern const char* const kSystemGetCloudUsersPath;
extern const char* const kSystemGetAccessRoleListPath;
extern const char* const kSystemRenamePath;
extern const char* const kSystemUpdatePath;
extern const char* const kSystemRecordUserSessionStartPath;
extern const char* const kSystemHealthHistoryPath;

extern const char* const kAuthGetNoncePath;
extern const char* const kAuthGetAuthenticationPath;

extern const char* const kSubscribeToSystemEventsPath;

extern const char* const kPingPath;

extern const char* const kEstablishEc2TransactionConnectionPath;
extern const char* const kDeprecatedEstablishEc2TransactionConnectionPath;
extern const char* const kEstablishEc2P2pTransactionConnectionPath;

// Maintenance.
extern const char* const kMaintenanceGetVmsConnections;
extern const char* const kMaintenanceGetTransactionLog;
extern const char* const kMaintenanceGetStatistics;

extern const char* const kDeprecatedCloudModuleXmlPath;

extern const char* const kDiscoveryCloudModuleXmlPath;

} // namespace cdb
} // namespace nx
