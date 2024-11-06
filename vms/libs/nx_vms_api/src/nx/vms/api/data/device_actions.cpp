// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_actions.h"

#include <nx/fusion/model_functions.h>

namespace QJson {

template<>
bool deserialize(
    QnJsonContext* context, const QJsonValue& value, nx::vms::api::AnalyticsFilter* outTarget)
{
    if (value.isObject())
        return QnSerialization::deserialize(context, value, outTarget);

    if (!value.isString())
        return false;

    QByteArray serialized = value.toString().toUtf8();
    return QJson::deserialize(context, serialized, outTarget);
}

} // namespace QJson

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DevicePasswordRequest, (json), DevicePasswordRequest_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AnalyticsFilter, (json), AnalyticsFilter_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceFootageRequest, (json), DeviceFootageRequest_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceDiagnosis, (json), DeviceDiagnosis_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceResourceData, (json), DeviceResourceData_Fields)

DeprecatedFieldNames const * AnalyticsFilter::getDeprecatedFieldNames()
{
    // Since /rest/v4.
    const static DeprecatedFieldNames names{{"maxAnalyticsDetailsMs", "maxAnalyticsDetails"}};
    return &names;
}

} // namespace nx::vms::api
