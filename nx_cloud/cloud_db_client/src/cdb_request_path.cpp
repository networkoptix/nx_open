/**********************************************************
* Oct 5, 2015
* akolesnikov
***********************************************************/

#include "cdb_request_path.h"


namespace nx {
namespace cdb {

const char* kAccountRegisterPath = "/account/register";
const char* kAccountActivatePath = "/account/activate";
const char* kAccountGetPath = "/account/get";
const char* kAccountUpdatePath = "/account/update";
const char* kAccountPasswordResetPath = "/account/reset_password";
const char* kAccountReactivatePath = "/account/reactivate";

const char* kSystemBindPath = "/system/bind";
const char* kSystemUnbindPath = "/system/unbind";
const char* kSystemGetPath = "/system/get";
const char* kSystemSharePath = "/system/share";
const char* kSystemGetCloudUsersPath = "/system/get_cloud_users";
const char* kSystemGetAccessRoleListPath = "/system/get_access_role_list";

const char* kAuthGetNoncePath = "/auth/get_nonce";
const char* kAuthGetAuthenticationPath = "/auth/get_authentication";

const char* kPingPath = "/ping";

}   //cdb
}   //nx
