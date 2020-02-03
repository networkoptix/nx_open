#pragma once

#include <nx/vms/api/types_fwd.h>

namespace nx::vms::api {

enum MotionType
{
    MT_Default = 0x0,
    MT_HardwareGrid = 0x1,
    MT_SoftwareGrid = 0x2,
    MT_MotionWindow = 0x4,
    MT_NoMotion = 0x8
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(MotionType)

Q_DECLARE_FLAGS(MotionTypes, MotionType)
Q_DECLARE_OPERATORS_FOR_FLAGS(MotionTypes)

enum class StreamIndex
{
    /**%apidoc
     * Stream is not defined.
     */
    undefined = -1,

    /**%apidoc
     * Primary stream.
     */
    primary = 0,

    /**%apidoc
     * Secondary stream.
     */
    secondary = 1
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(StreamIndex)

StreamIndex NX_VMS_API oppositeStreamIndex(StreamIndex streamIndex);

} // namespace nx::vms::api

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::MotionType,
    (metatype)(numeric)(lexical)(debug),
    NX_VMS_API)

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::MotionTypes,
    (metatype)(numeric)(lexical)(debug),
    NX_VMS_API)
