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

} // namespace api
} // namespace vms
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::EventReason, (metatype)(lexical), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::EventState, (metatype)(lexical), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::ResourceStatus, (metatype)(lexical), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::RecordingType, (metatype)(lexical), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::StreamQuality, (metatype)(lexical), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::FailoverPriority, (metatype)(lexical), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::IoModuleVisualStyle, (metatype)(lexical), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::ConnectionType, (metatype)(lexical), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::RtpTransportType, (metatype)(lexical), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::MotionStreamType, (metatype)(lexical), NX_VMS_API)
