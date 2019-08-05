#include "engine_manifest.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::analytics::EngineManifest, Capability,
    (nx::vms::api::analytics::EngineManifest::Capability::noCapabilities, "noCapabilities")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_yuv420, "needUncompressedVideoFrames_yuv420")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_argb, "needUncompressedVideoFrames_argb")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_abgr, "needUncompressedVideoFrames_abgr")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_rgba, "needUncompressedVideoFrames_rgba")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_bgra, "needUncompressedVideoFrames_bgra")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_rgb, "needUncompressedVideoFrames_rgb")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_bgr, "needUncompressedVideoFrames_bgr")
    (nx::vms::api::analytics::EngineManifest::Capability::deviceDependent, "deviceDependent")
)
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::analytics::EngineManifest::Capability, (numeric)(debug))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::analytics::EngineManifest, Capabilities,
    (nx::vms::api::analytics::EngineManifest::Capability::noCapabilities, "noCapabilities")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_yuv420, "needUncompressedVideoFrames_yuv420")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_argb, "needUncompressedVideoFrames_argb")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_abgr, "needUncompressedVideoFrames_abgr")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_rgba, "needUncompressedVideoFrames_rgba")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_bgra, "needUncompressedVideoFrames_bgra")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_rgb, "needUncompressedVideoFrames_rgb")
    (nx::vms::api::analytics::EngineManifest::Capability::needUncompressedVideoFrames_bgr, "needUncompressedVideoFrames_bgr")
    (nx::vms::api::analytics::EngineManifest::Capability::deviceDependent, "deviceDependent")
)
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::analytics::EngineManifest::Capabilities, (numeric)(debug))

//-------------------------------------------------------------------------------------------------

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
    nx::vms::api::analytics::EngineManifest::ObjectAction, Capability,
    (nx::vms::api::analytics::EngineManifest::ObjectAction::Capability::noCapabilities,
        "noCapabilities")
    (nx::vms::api::analytics::EngineManifest::ObjectAction::Capability::needBestShotVideoFrame,
        "needBestShotVideoFrame")
    (nx::vms::api::analytics::EngineManifest::ObjectAction::Capability::needBestShotObjectMetadata,
        "needBestShotObjectMetadata")
    (nx::vms::api::analytics::EngineManifest::ObjectAction::Capability::needTrack, "needTrack")
)
QN_FUSION_DEFINE_FUNCTIONS(
    nx::vms::api::analytics::EngineManifest::ObjectAction::Capability, (numeric)(debug))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
    nx::vms::api::analytics::EngineManifest::ObjectAction, Capabilities,
    (nx::vms::api::analytics::EngineManifest::ObjectAction::Capability::noCapabilities,
        "noCapabilities")
    (nx::vms::api::analytics::EngineManifest::ObjectAction::Capability::needBestShotVideoFrame,
        "needBestShotVideoFrame")
    (nx::vms::api::analytics::EngineManifest::ObjectAction::Capability::needBestShotObjectMetadata,
        "needBestShotObjectMetadata")
    (nx::vms::api::analytics::EngineManifest::ObjectAction::Capability::needTrack, "needTrack")
)
QN_FUSION_DEFINE_FUNCTIONS(
    nx::vms::api::analytics::EngineManifest::ObjectAction::Capabilities, (numeric)(debug))

//-------------------------------------------------------------------------------------------------

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EngineManifest::ObjectAction::Requirements, (json)(eq),
    nx_vms_api_analytics_Engine_ObjectAction_Requirements_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EngineManifest::ObjectAction, (json),
    ObjectAction_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EngineManifest, (json),
    EngineManifest_Fields, (brief, true))

} // namespace nx::vms::api::analytics
