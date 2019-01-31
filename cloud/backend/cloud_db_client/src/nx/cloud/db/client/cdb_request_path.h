#pragma once

namespace nx::cloud::db {

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
extern const char* const kSystemsMergedToASpecificSystem;

extern const char* const kAuthGetNoncePath;
extern const char* const kAuthGetAuthenticationPath;

extern const char* const kSubscribeToSystemEventsPath;

extern const char* const kPingPath;

extern const char* const kEc2TransactionConnectionPathPrefix;
extern const char* const kDeprecatedEc2TransactionConnectionPathPrefix;

// Maintenance.
extern const char* const kMaintenanceGetVmsConnections;
extern const char* const kMaintenanceGetTransactionLog;
extern const char* const kMaintenanceGetStatistics;

// Statistics.
extern const char* const kStatisticsMetricsPath;

extern const char* const kDeprecatedCloudModuleXmlPath;
// TODO: #ak Added because of lack of URL rewrite support in Amazon balancer.
// Remove kDeprecatedCloudModuleXmlPath after switching to new balancer on every instance.
extern const char* const kAnotherDeprecatedCloudModuleXmlPath;

extern const char* const kDiscoveryCloudModuleXmlPath;

extern const char* const kApiPrefix;

extern const char* const kSystemIdParam;

} // namespace nx::cloud::db
