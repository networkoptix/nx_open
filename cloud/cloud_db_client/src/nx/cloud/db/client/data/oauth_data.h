// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/cloud/db/api/oauth_data.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/url_query.h>

#include "account_data.h"

namespace nx::cloud::db::api {

NX_REFLECTION_INSTRUMENT(IssuePasswordResetCodeRequest, (account)(codeExpirationTimeout))

} // namespace nx::cloud::db::api
