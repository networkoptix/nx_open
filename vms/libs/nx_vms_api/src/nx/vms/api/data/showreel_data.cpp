// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "showreel_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ShowreelItemData, (ubjson)(xml)(json)(sql_record)(csv_record), ShowreelItemData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ShowreelData, (ubjson)(xml)(json)(sql_record)(csv_record), ShowreelData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ShowreelSettings, (ubjson)(xml)(json)(csv_record), ShowreelSettings_Fields)

} // namespace nx::vms::api

void serialize_field(const nx::vms::api::ShowreelSettings& settings, QVariant* target)
{
    serialize_field(QJson::serialized(settings), target);
}

void deserialize_field(const QVariant& value, nx::vms::api::ShowreelSettings* target)
{
    QByteArray tmp;
    deserialize_field(value, &tmp);
    QJson::deserialize(tmp, target);
}
