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
enum class DayOfWeek;
enum class BackupType;
enum class UserRole;

} // namespace api
} // namespace vms
} // namespace nx

#define API_DECLARE_TYPE(type) \
    QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::type, (metatype)(lexical)(debug), NX_VMS_API)

API_DECLARE_TYPE(EventReason)
API_DECLARE_TYPE(EventState)
API_DECLARE_TYPE(ResourceStatus)
API_DECLARE_TYPE(RecordingType)
API_DECLARE_TYPE(StreamQuality)
API_DECLARE_TYPE(FailoverPriority)
API_DECLARE_TYPE(IoModuleVisualStyle)
API_DECLARE_TYPE(ConnectionType)
API_DECLARE_TYPE(RtpTransportType)
API_DECLARE_TYPE(MotionStreamType)
API_DECLARE_TYPE(PeerType)
API_DECLARE_TYPE(BackupType)
API_DECLARE_TYPE(UserRole)
