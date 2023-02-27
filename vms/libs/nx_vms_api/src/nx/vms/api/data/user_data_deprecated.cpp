// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_data_deprecated.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/std/algorithm.h>

namespace nx::vms::api {

void UserDataDeprecated::fillId()
{
    // ATTENTION: This logic is similar to QnUserResource::fillId().
    if (isCloud)
    {
        if (!email.isEmpty())
            id = QnUuid::fromArbitraryData(email);
        else
            id = QnUuid();
    }
    else
    {
        id = QnUuid::createUuid();
    }
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    UserDataDeprecated, (ubjson)(json)(xml)(sql_record)(csv_record), UserDataDeprecated_Fields)

} // namespace nx::vms::api
