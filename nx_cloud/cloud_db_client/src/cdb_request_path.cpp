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

const char* kSystemBindPath = "/system/bind";
const char* kSystemUnbindPath = "/system/unbind";
const char* kSystemGetPath = "/system/get";
const char* kSystemSharePath = "/system/share";
const char* kSystemGetCloudUsersPath = "/system/get_cloud_users";
const char* kSystemUpdateSharingPath = "/system/update_sharing";

const char* kAuthGetNoncePath = "/auth/get_nonce";
const char* kAuthGetAuthenticationPath = "/auth/get_authentication";

const char* kPingPath = "/ping";

}
}
