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
struct CameraAttributesDataBackwardCompatibility: public CameraAttributesData
{
    bool licenseUsed = false;
    std::optional<int> maxArchiveDays;
    std::optional<int> minArchiveDays;
};

#define CameraAttributesDataBackwardCompatibility_Fields CameraAttributesData_Fields (licenseUsed)(maxArchiveDays)(minArchiveDays)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CameraAttributesDataBackwardCompatibility,
    (json), CameraAttributesDataBackwardCompatibility_Fields)

void serialize(QnJsonContext* ctx, const CameraAttributesData& value, QJsonValue* target)
{
    CameraAttributesDataBackwardCompatibility compatibilityValue;
    static_cast<CameraAttributesData&>(compatibilityValue) = value;
    compatibilityValue.licenseUsed = value.scheduleEnabled;
    using namespace std::chrono;
    compatibilityValue.maxArchiveDays = duration_cast<days>(value.maxArchivePeriodS).count();
    compatibilityValue.minArchiveDays = duration_cast<days>(value.minArchivePeriodS).count();
    ctx->setChronoSerializedAsDouble(true);
    serialize(ctx, compatibilityValue, target);
}

bool deserialize(QnJsonContext* ctx, const QJsonValue& value, CameraAttributesData* target)
{
    CameraAttributesDataBackwardCompatibility cValue;
    bool result = deserialize(ctx, value, &cValue);
    if (!result)
        return false;

    *target = cValue;
    using namespace std::chrono;
    if (cValue.minArchiveDays.has_value()
        && duration_cast<days>(cValue.minArchivePeriodS) != days(*cValue.minArchiveDays))
    {
        target->minArchivePeriodS = duration_cast<seconds>(days(*cValue.minArchiveDays));
    }
    if (cValue.maxArchiveDays.has_value()
        && duration_cast<days>(cValue.maxArchivePeriodS) != days(*cValue.maxArchiveDays))
    {
        target->maxArchivePeriodS = duration_cast<seconds>(days(*cValue.maxArchiveDays));
    }

    return result;
}

} // namespace api
} // namespace vms
} // namespace nx
