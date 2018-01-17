#include "cdb_request_path.h"

namespace nx {
namespace cdb {

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

// TODO: These values are coupled with nx::cdb::api::kEc2EventsPath in "ec2_request_paths.h".
const char* const kEstablishEc2TransactionConnectionPath = "/cdb/ec2/events/ConnectingStage1";
const char* const kDeprecatedEstablishEc2TransactionConnectionPath = "/ec2/events/ConnectingStage1";
const char* const kEstablishEc2P2pTransactionConnectionPath = "/cdb/ec2/messageBus";

const char* const kMaintenanceGetVmsConnections = "/cdb/maintenance/vmsConnections";
const char* const kMaintenanceGetTransactionLog = "/cdb/maintenance/transactionLog";
const char* const kMaintenanceGetStatistics = "/cdb/maintenance/statistics";

const char* const kDeprecatedCloudModuleXmlPath = "/cdb/cloud_modules.xml";

const char* const kDiscoveryCloudModuleXmlPath = "/discovery/cloud_modules.xml";

} // namespace cdb
} // namespace nx
