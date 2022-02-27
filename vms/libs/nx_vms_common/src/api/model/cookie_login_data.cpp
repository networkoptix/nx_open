// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cookie_login_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/random.h>

#include <nx/utils/crypt/linux_passwd_crypt.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCookieData, (json), QnCookieData_Fields)
