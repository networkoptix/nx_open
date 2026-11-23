// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <set>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/vms/api/data/resolution_data.h>
#include <nx/vms/api/types/motion_types.h>
#include <nx/vms/api/types/streaming_protocol.h>

namespace nx::vms::api {

struct NX_VMS_API CameraStreamCapability
{
    float minBitrateKbps = 0;
    float maxBitrateKbps = 0;
    float defaultBitrateKbps = 0;
    float defaultFps = 0;

    /**%apidoc
     * minFps/maxFps are optional; when set, they take priority over availableFps.
     */
    float minFps = 0.0;
    float maxFps = 0.0;

    /**%apidoc:{std::vector<double>}
     * If empty, the incoming fps is clamped to the range [minFps; maxFps].
     */
    std::set<double> availableFps;

    QString toString() const;
    float calcMaxFps() const noexcept;
    void setRangeFps(float maxFps, float minFps = 1.0);
    float strictFps(float fps) const noexcept;
    bool isRangeFpsValid() const noexcept;
};
#define CameraStreamCapability_Fields (minBitrateKbps)(maxBitrateKbps)(defaultBitrateKbps)(defaultFps)(minFps)(maxFps)(availableFps)
QN_FUSION_DECLARE_FUNCTIONS(CameraStreamCapability, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(CameraStreamCapability, CameraStreamCapability_Fields)

struct NX_VMS_API CameraMediaCapability
{
    std::map<nx::vms::api::StreamIndex, CameraStreamCapability> streamCapabilities;
    bool hasDualStreaming = false;
    bool hasAudio = false;
    /**%apidoc:stringArray
     * Protocols that can currently be used to read the Device media streams. Empty if none can
     * be used, or if the Device driver doesn't read the streams with these protocols.
     * Protocols the Device supports but has disabled are not listed. When camera control is
     * allowed, the Server enables a disabled protocol on the Device initialization if the stream
     * security policy would use it (e.g. a secure one if the Site requires secure streaming).
     * %example ["rtsp", "rtsps"]
     */
    std::set<StreamingProtocol> streamingProtocols;

    /**%apidoc:string
     * Resolution in format `{width}x{height}`. The string either may contain width
     * and height (for instance 320x240) or height only (for instance 240p).
     * By default, 640x480 is used.
     */
    nx::vms::api::ResolutionData maxResolution;

    // TODO: move more fields to here like io port settings e.t.c
};
#define CameraMediaCapability_Fields \
    (streamCapabilities)(hasDualStreaming)(hasAudio) (streamingProtocols) (maxResolution)
QN_FUSION_DECLARE_FUNCTIONS(CameraMediaCapability, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(CameraMediaCapability, CameraMediaCapability_Fields)

NX_REFLECTION_ENUM_CLASS(CameraTraitType,
    aspectRatioDependent = 1
)

// TODO: #dmishin it's workaround for fusion bug (impossibility to serialize/deserialize maps
// with the key different from QString). Should be fixed in 4.0
using CameraTraitNameString = QString;

using CameraTraitAttributes = std::map<QString, QString>;
using CameraTraits = std::map<CameraTraitNameString, CameraTraitAttributes>;

} // namespace nx::vms::api
