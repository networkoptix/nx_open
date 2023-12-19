// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/vms/api/data/resolution_data.h>
#include <nx/vms/api/types/motion_types.h>

namespace nx::vms::api {

struct NX_VMS_API CameraStreamCapability
{
    float minBitrateKbps = 0;
    float maxBitrateKbps = 0;
    float defaultBitrateKbps = 0;
    int defaultFps = 0;
    int maxFps = 0;

    CameraStreamCapability(float minBitrate = 0.0, float maxBitrate = 0.0, int fps = 0);
    bool isNull() const;
    QString toString() const;
};
#define CameraStreamCapability_Fields (minBitrateKbps)(maxBitrateKbps)(defaultBitrateKbps)(defaultFps)(maxFps)
QN_FUSION_DECLARE_FUNCTIONS(CameraStreamCapability, (json), NX_VMS_API)

struct NX_VMS_API CameraMediaCapability
{
    std::map<nx::vms::api::StreamIndex, CameraStreamCapability> streamCapabilities;
    bool hasDualStreaming = false;
    bool hasAudio = false;

    /**%apidoc:string
     * Resolution in format `{width}x{height}`. The string either may contain width
     * and height (for instance 320x240) or height only (for instance 240p).
     * By default, 640x480 is used.
     */
    nx::vms::api::ResolutionData maxResolution;

    // TODO: move more fields to here like io port settings e.t.c
};
#define CameraMediaCapability_Fields (streamCapabilities)(hasDualStreaming)(hasAudio)(maxResolution)
QN_FUSION_DECLARE_FUNCTIONS(CameraMediaCapability, (json), NX_VMS_API)

NX_REFLECTION_ENUM_CLASS(CameraTraitType,
    aspectRatioDependent = 1
)

// TODO: #dmishin it's workaround for fusion bug (impossibility to serialize/deserialize maps
// with the key different from QString). Should be fixed in 4.0
using CameraTraitNameString = QString;

using CameraTraitAttributes = std::map<QString, QString>;
using CameraTraits = std::map<CameraTraitNameString, CameraTraitAttributes>;

} // namespace nx::vms::api
