// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_data_ex.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    CameraDataEx, (ubjson)(xml)(csv_record)(sql_record), CameraDataEx_Fields)

// TODO #lbusygin: remove backward compatibility support in 6.0
namespace detail {

QN_FUSION_DEFINE_FUNCTIONS(CameraDataEx, (json))

} // namespace detail

struct CameraDataExBackwardCompatibility: public CameraDataEx
{
    bool licenseUsed = false;
    int maxArchiveDays = 0;
    int minArchiveDays = 0;
};

#define CameraDataExBackwardCompatibility_Fields CameraDataEx_Fields (licenseUsed)(maxArchiveDays)(minArchiveDays)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CameraDataExBackwardCompatibility,
    (json), CameraDataExBackwardCompatibility_Fields)

void serialize(QnJsonContext* ctx, const CameraDataEx& value, QJsonValue* target)
{
    CameraDataExBackwardCompatibility compatibilityValue;
    static_cast<CameraDataEx&>(compatibilityValue) = value;
    compatibilityValue.licenseUsed = value.scheduleEnabled;
    using namespace std::chrono;
    compatibilityValue.maxArchiveDays = duration_cast<days>(value.maxArchivePeriodS).count();
    compatibilityValue.minArchiveDays = duration_cast<days>(value.minArchivePeriodS).count();
    ctx->setChronoSerializedAsDouble(true);
    serialize(ctx, compatibilityValue, target);
}

bool deserialize(QnJsonContext *ctx, const QJsonValue &value, CameraDataEx *target)
{
    return detail::deserialize(ctx, value, target);
}


} // namespace api
} // namespace vms
} // namespace nx
