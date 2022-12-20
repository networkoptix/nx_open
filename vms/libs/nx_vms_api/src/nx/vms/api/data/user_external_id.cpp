// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_external_id.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    UserExternalId, (ubjson)(xml)(json)(sql_record)(csv_record), UserExternalId_Fields)

void serialize_field(const UserExternalId& from, QVariant* to)
{
    *to = from.isEmpty() ? QString() : QString(QJson::serialized(from));
}

void deserialize_field(const QVariant& from, UserExternalId* to)
{
    QJson::deserialize(from.toString(), to);
}

} // namespace nx::vms::api

