#pragma once

#include <nx/vms/api/types_fwd.h>

namespace nx::vms::api {

/**%apidoc
 * Type of motion detection method.
 */
enum MotionType
{
    /**%apidoc Use default method.
     * %caption 0
     */
    MT_Default = 0,

    /**%apidoc Use motion detection grid implemented by the camera.
     * %caption 1
     */
    MT_HardwareGrid = 1 << 0,

    /**%apidoc Use motion detection grid implemented by the server.
     * %caption 2
     */
    MT_SoftwareGrid = 1 << 1,

    /**%apidoc Use motion detection window implemented by the camera.
     * %caption 4
     */
    MT_MotionWindow = 1 << 2,

    /**%apidoc Do not perform motion detection.
     * %caption 8
     */
    MT_NoMotion = 1 << 3,
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(MotionType)

Q_DECLARE_FLAGS(MotionTypes, MotionType)
Q_DECLARE_OPERATORS_FOR_FLAGS(MotionTypes)

enum class StreamIndex
{
    undefined = -1,
    primary = 0,
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
