#pragma once

namespace nx::cloud::db {

static constexpr char kAccountRegisterPath[] = "/cdb/account/register";
static constexpr char kAccountActivatePath[] = "/cdb/account/activate";
static constexpr char kAccountGetPath[] = "/cdb/account/get";
static constexpr char kAccountUpdatePath[] = "/cdb/account/update";
static constexpr char kAccountPasswordResetPath[] = "/cdb/account/resetPassword";
static constexpr char kAccountReactivatePath[] = "/cdb/account/reactivate";
static constexpr char kAccountCreateTemporaryCredentialsPath[] = "/cdb/account/createTemporaryCredentials";

static constexpr char kSystemBindPath[] = "/cdb/system/bind";
static constexpr char kSystemUnbindPath[] = "/cdb/system/unbind";
static constexpr char kSystemGetPath[] = "/cdb/system/get";
static constexpr char kSystemSharePath[] = "/cdb/system/share";
static constexpr char kSystemGetCloudUsersPath[] = "/cdb/system/getCloudUsers";
static constexpr char kSystemGetAccessRoleListPath[] = "/cdb/system/getAccessRoleList";
static constexpr char kSystemRenamePath[] = "/cdb/system/rename";
static constexpr char kSystemUpdatePath[] = "/cdb/system/update";
static constexpr char kSystemRecordUserSessionStartPath[] = "/cdb/system/recordUserSessionStart";
static constexpr char kSystemHealthHistoryPath[] = "/cdb/system/healthHistory";
static constexpr char kSystemsMergedToASpecificSystem[] = "/cdb/system/{systemId}/merged_systems/";

static constexpr char kAuthGetNoncePath[] = "/cdb/auth/getNonce";
static constexpr char kAuthGetAuthenticationPath[] = "/cdb/auth/getAuthentication";
static constexpr char kAuthResolveUserCredentials[] = "/cdb/auth_provider/caller-identity";
static constexpr char kAuthSystemAccessLevel[] = "/cdb/auth_provider/system/{systemId}/access-level";

static constexpr char kSubscribeToSystemEventsPath[] = "/cdb/event/subscribe";

static constexpr char kPingPath[] = "/cdb/ping";

static constexpr char kEc2TransactionConnectionPathPrefix[] = "/ec2/";
static constexpr char kDeprecatedEc2TransactionConnectionPathPrefix[] = "/cdb/ec2/";

static constexpr char kMaintenanceGetVmsConnections[] = "/cdb/maintenance/vmsConnections";
static constexpr char kMaintenanceGetTransactionLog[] = "/cdb/maintenance/transactionLog";
static constexpr char kMaintenanceGetStatistics[] = "/cdb/maintenance/statistics";

static constexpr char kStatisticsPath[] = "/cdb/statistics";
static constexpr char kStatisticsMetricsPath[] = "/cdb/statistics/metrics/";

static constexpr char kDeprecatedCloudModuleXmlPath[] = "/cdb/cloud_modules.xml";
static constexpr char kAnotherDeprecatedCloudModuleXmlPath[] = "/api/cloud_modules.xml";

static constexpr char kDiscoveryCloudModuleXmlPath[] = "/discovery/v1/cloud_modules.xml";

static constexpr char kApiPrefix[] = "/cdb";

static constexpr char kSystemIdParam[] = "systemId";

} // namespace nx::cloud::db
