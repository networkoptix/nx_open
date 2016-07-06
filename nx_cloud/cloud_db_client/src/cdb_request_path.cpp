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
const char* kAccountPasswordResetPath = "/cdb/account/reset_password";
const char* kAccountReactivatePath = "/cdb/account/reactivate";
const char* kAccountCreateTemporaryCredentialsPath = "/cdb/account/create_temporary_credentials";

const char* kSystemBindPath = "/cdb/system/bind";
const char* kSystemUnbindPath = "/cdb/system/unbind";
const char* kSystemGetPath = "/cdb/system/get";
const char* kSystemSharePath = "/cdb/system/share";
const char* kSystemSetSystemUserListPath = "/cdb/system/set_system_user_list";
const char* kSystemGetCloudUsersPath = "/cdb/system/get_cloud_users";
const char* kSystemGetAccessRoleListPath = "/cdb/system/get_access_role_list";
const char* kSystemUpdateSystemNamePath = "/cdb/system/update_system_name";

const char* kAuthGetNoncePath = "/cdb/auth/get_nonce";
const char* kAuthGetAuthenticationPath = "/cdb/auth/get_authentication";

const char* kSubscribeToSystemEventsPath = "/cdb/event/subscribe";

const char* kPingPath = "/cdb/ping";

}   //cdb
}   //nx
