// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_attributes_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

DeprecatedFieldNames* CameraAttributesData::getDeprecatedFieldNames()
{
    static DeprecatedFieldNames kDeprecatedFieldNames{
        {"cameraId", "cameraID"},                  //< up to v2.6
        {"preferredServerId", "preferedServerId"}, //< up to v2.6
    };
    return &kDeprecatedFieldNames;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ScheduleTaskData, (ubjson)(xml)(json)(sql_record)(csv_record), ScheduleTaskData_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CameraAttributesData,
    (ubjson)(xml)(sql_record)(csv_record), CameraAttributesData_Fields)

// TODO #lbusygin: remove backward compatibility support in 5.2
namespace detail {

QN_FUSION_DEFINE_FUNCTIONS(CameraAttributesData, (json))

} // namespace detail

struct CameraAttributesDataBackwardCompatibility: public CameraAttributesData
{
    bool licenseUsed = false;
};

#define CameraAttributesDataBackwardCompatibility_Fields CameraAttributesData_Fields (licenseUsed)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CameraAttributesDataBackwardCompatibility,
    (json), CameraAttributesDataBackwardCompatibility_Fields)

void serialize(QnJsonContext* ctx, const CameraAttributesData& value, QJsonValue* target)
{
    CameraAttributesDataBackwardCompatibility compatibilityValue;
    static_cast<CameraAttributesData&>(compatibilityValue) = value;
    compatibilityValue.licenseUsed = value.scheduleEnabled;
    serialize(ctx, compatibilityValue, target);
}

bool deserialize(QnJsonContext* ctx, const QJsonValue& value, CameraAttributesData* target)
{
    return detail::deserialize(ctx, value, target);
}

} // namespace api
} // namespace vms
} // namespace nx
