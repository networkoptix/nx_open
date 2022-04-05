// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_role_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QString UserRoleData::toString() const
{
    return QJson::serialized(*this);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    UserRoleData, (ubjson)(json)(xml)(sql_record)(csv_record), UserRoleData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    PredefinedRoleData, (ubjson)(json)(xml)(sql_record)(csv_record), PredefinedRoleData_Fields)

} // namespace nx::vms::api
