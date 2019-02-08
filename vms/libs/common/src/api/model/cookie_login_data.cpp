#include "cookie_login_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/random.h>

#include <utils/common/app_info.h>
#include <nx/utils/crypt/linux_passwd_crypt.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((QnCookieData), (json), _Fields)
