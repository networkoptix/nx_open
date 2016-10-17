/**********************************************************
* Oct 5, 2015
* akolesnikov
***********************************************************/

#include "cdb_request_path.h"


namespace nx {
namespace cdb {

const char* kAccountRegisterPath = "/cdb/account/register";
const char* kAccountActivatePath = "/cdb/account/activate";
const char* kAccountGetPath = "/cdb/account/get";
const char* kAccountUpdatePath = "/cdb/account/update";
const char* kAccountPasswordResetPath = "/cdb/account/resetPassword";
const char* kAccountReactivatePath = "/cdb/account/reactivate";
const char* kAccountCreateTemporaryCredentialsPath = "/cdb/account/createTemporaryCredentials";

const char* kSystemBindPath = "/cdb/system/bind";
const char* kSystemUnbindPath = "/cdb/system/unbind";
const char* kSystemGetPath = "/cdb/system/get";
const char* kSystemSharePath = "/cdb/system/share";
const char* kSystemGetCloudUsersPath = "/cdb/system/getCloudUsers";
const char* kSystemGetAccessRoleListPath = "/cdb/system/getAccessRoleList";
const char* kSystemRenamePath = "/cdb/system/rename";
const char* kSystemUpdatePath = "/cdb/system/update";
const char* kSystemRecordUserSessionStartPath = "/cdb/system/recordUserSessionStart";

const char* kAuthGetNoncePath = "/cdb/auth/getNonce";
const char* kAuthGetAuthenticationPath = "/cdb/auth/getAuthentication";

const char* kSubscribeToSystemEventsPath = "/cdb/event/subscribe";

const char* kPingPath = "/cdb/ping";

const char* kEstablishEc2TransactionConnectionPath = "/ec2/events/ConnectingStage1";
const char* kPushEc2TransactionPath = "/ec2/forward_events/";

const char* kMaintenanceGetVmsConnections = "/cdb/maintenance/getVmsConnections";
const char* kMaintenanceGetTransactionLog = "/cdb/maintenance/getTransactionLog";

}   //cdb
}   //nx
