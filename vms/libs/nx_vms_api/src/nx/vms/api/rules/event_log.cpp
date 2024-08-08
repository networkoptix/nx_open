// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_log.h"

#include <nx/fusion/model_functions.h>
#include <nx/reflect/json.h>
#include <nx/utils/buffer.h>
#include <nx/utils/serialization/qjson.h>
#include <nx/utils/serialization/qt_containers_reflect_json.h>
#include <nx/utils/serialization/qt_core_types.h>

namespace nx::vms::api::rules {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EventLogFilter, (json), EventLogFilter_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EventLogRecord, (json)(sql_record), EventLogRecord_Fields)

std::string toString(const EventLogRecord& record)
{
    return nx::reflect::json::serialize(record);
}

} // namespace nx::vms::rules

void serialize_field(const nx::vms::api::rules::PropertyMap& data, QVariant* value)
{
    *value = QByteArray::fromStdString(nx::reflect::json::serialize(data));
}

void deserialize_field(const QVariant& value, nx::vms::api::rules::PropertyMap* data)
{
    nx::reflect::json::deserialize(nx::toBufferView(value.toByteArray()), data);
}
