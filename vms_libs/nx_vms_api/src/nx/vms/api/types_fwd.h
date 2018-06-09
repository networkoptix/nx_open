#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms {
namespace api {

enum class EventReason;
enum class EventState;
enum class ResourceStatus;
enum class RecordingType;
enum class StreamQuality;
enum class FailoverPriority;
enum class IoModuleVisualStyle;
enum class ConnectionType;
enum class RtpTransportType;
enum class MotionStreamType;
enum class PeerType;

} // namespace api
} // namespace vms
} // namespace nx

#define DECLARE_TYPE(type) \
    QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::type, (metatype)(lexical)(debug), NX_VMS_API)

DECLARE_TYPE(EventReason)
DECLARE_TYPE(EventState)
DECLARE_TYPE(ResourceStatus)
DECLARE_TYPE(RecordingType)
DECLARE_TYPE(StreamQuality)
DECLARE_TYPE(FailoverPriority)
DECLARE_TYPE(IoModuleVisualStyle)
DECLARE_TYPE(ConnectionType)
DECLARE_TYPE(RtpTransportType)
DECLARE_TYPE(MotionStreamType)
DECLARE_TYPE(PeerType)

#undef DECLARE_TYPE
