#include "cdb_request_path.h"

namespace nx::cloud::db {

const char* const kAccountRegisterPath = "/cdb/account/register";
const char* const kAccountActivatePath = "/cdb/account/activate";
const char* const kAccountGetPath = "/cdb/account/get";
const char* const kAccountUpdatePath = "/cdb/account/update";
const char* const kAccountPasswordResetPath = "/cdb/account/resetPassword";
const char* const kAccountReactivatePath = "/cdb/account/reactivate";
const char* const kAccountCreateTemporaryCredentialsPath = "/cdb/account/createTemporaryCredentials";

const char* const kSystemBindPath = "/cdb/system/bind";
const char* const kSystemUnbindPath = "/cdb/system/unbind";
const char* const kSystemGetPath = "/cdb/system/get";
const char* const kSystemSharePath = "/cdb/system/share";
const char* const kSystemGetCloudUsersPath = "/cdb/system/getCloudUsers";
const char* const kSystemGetAccessRoleListPath = "/cdb/system/getAccessRoleList";
const char* const kSystemRenamePath = "/cdb/system/rename";
const char* const kSystemUpdatePath = "/cdb/system/update";
const char* const kSystemRecordUserSessionStartPath = "/cdb/system/recordUserSessionStart";
const char* const kSystemHealthHistoryPath = "/cdb/system/healthHistory";
const char* const kSystemsMergedToASpecificSystem = "/cdb/system/{systemId}/merged_systems/";

const char* const kAuthGetNoncePath = "/cdb/auth/getNonce";
const char* const kAuthGetAuthenticationPath = "/cdb/auth/getAuthentication";

const char* const kSubscribeToSystemEventsPath = "/cdb/event/subscribe";

const char* const kPingPath = "/cdb/ping";

const char* const kEc2TransactionConnectionPathPrefix = "/ec2/";
const char* const kDeprecatedEc2TransactionConnectionPathPrefix = "/cdb/ec2/";

const char* const kMaintenanceGetVmsConnections = "/cdb/maintenance/vmsConnections";
const char* const kMaintenanceGetTransactionLog = "/cdb/maintenance/transactionLog";
const char* const kMaintenanceGetStatistics = "/cdb/maintenance/statistics";

const char* const kStatisticsMetricsPath = "/cdb/statistics/metrics/";

const char* const kDeprecatedCloudModuleXmlPath = "/cdb/cloud_modules.xml";
const char* const kAnotherDeprecatedCloudModuleXmlPath = "/api/cloud_modules.xml";

const char* const kDiscoveryCloudModuleXmlPath = "/discovery/v1/cloud_modules.xml";

const char* const kApiPrefix = "/cdb";

const char* const kSystemIdParam = "systemId";

} // namespace nx::cloud::db
