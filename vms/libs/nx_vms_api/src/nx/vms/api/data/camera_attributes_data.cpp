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

struct CameraAttributesDataBackwardCompatibility: public CameraAttributesData
{
    std::optional<int> maxArchiveDays;
    std::optional<int> minArchiveDays;
};

#define CameraAttributesDataBackwardCompatibility_Fields CameraAttributesData_Fields (maxArchiveDays)(minArchiveDays)

// TODO #rvasilenko: remove backward compatibility support in 5.2
namespace detail {

    QN_FUSION_DEFINE_FUNCTIONS(CameraAttributesDataBackwardCompatibility, (json))

} // namespace detail


QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CameraAttributesDataBackwardCompatibility,
    (json), CameraAttributesDataBackwardCompatibility_Fields)

void serialize(QnJsonContext* ctx, const CameraAttributesData& value, QJsonValue* target)
{
    CameraAttributesDataBackwardCompatibility compatibilityValue;
    static_cast<CameraAttributesData&>(compatibilityValue) = value;
    using namespace std::chrono;
    compatibilityValue.maxArchiveDays = duration_cast<days>(value.maxArchivePeriodS).count();
    compatibilityValue.minArchiveDays = duration_cast<days>(value.minArchivePeriodS).count();
    ctx->setChronoSerializedAsDouble(true);
    serialize(ctx, compatibilityValue, target);
}

bool deserialize(QnJsonContext* ctx, const QJsonValue& value, CameraAttributesData* target)
{
    CameraAttributesDataBackwardCompatibility compatibilityValue;
    bool result = detail::deserialize(ctx, value, &compatibilityValue);
    if (!result)
        return false;

    *target = compatibilityValue;
    using namespace std::chrono;
    if (compatibilityValue.minArchiveDays.has_value())
        target->minArchivePeriodS = duration_cast<seconds>(days(*compatibilityValue.minArchiveDays));
    if (compatibilityValue.maxArchiveDays.has_value())
        target->maxArchivePeriodS = duration_cast<seconds>(days(*compatibilityValue.maxArchiveDays));

    return result;
}

} // namespace api
} // namespace vms
} // namespace nx
