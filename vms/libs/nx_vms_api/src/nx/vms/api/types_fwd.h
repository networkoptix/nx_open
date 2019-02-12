#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::api {

enum class EventReason;
enum class EventState;
enum class ResourceStatus;
enum class RecordingType;
enum class StreamQuality;
enum class FailoverPriority;
enum class IoModuleVisualStyle;
enum class ConnectionType;
enum class RtpTransportType;
enum class StreamIndex;
enum class PeerType;
enum class DayOfWeek;
enum class BackupType;
enum class UserRole;

} // namespace nx::vms::api

#define NX_VMS_API_DECLARE_TYPE(type) \
    QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::type, (metatype)(lexical)(debug), NX_VMS_API)

NX_VMS_API_DECLARE_TYPE(EventReason)
NX_VMS_API_DECLARE_TYPE(EventState)
NX_VMS_API_DECLARE_TYPE(ResourceStatus)
NX_VMS_API_DECLARE_TYPE(RecordingType)
NX_VMS_API_DECLARE_TYPE(StreamQuality)
NX_VMS_API_DECLARE_TYPE(FailoverPriority)
NX_VMS_API_DECLARE_TYPE(IoModuleVisualStyle)
NX_VMS_API_DECLARE_TYPE(ConnectionType)
NX_VMS_API_DECLARE_TYPE(RtpTransportType)
NX_VMS_API_DECLARE_TYPE(StreamIndex)
NX_VMS_API_DECLARE_TYPE(PeerType)
NX_VMS_API_DECLARE_TYPE(BackupType)
NX_VMS_API_DECLARE_TYPE(UserRole)
