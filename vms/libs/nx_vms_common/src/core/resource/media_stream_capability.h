// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>

#include <common/common_globals.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>

namespace nx {
namespace media {

struct NX_VMS_COMMON_API CameraStreamCapability
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
QN_FUSION_DECLARE_FUNCTIONS(CameraStreamCapability, (json), NX_VMS_COMMON_API)

struct CameraMediaCapability
{
    QMap<nx::vms::api::StreamIndex, CameraStreamCapability> streamCapabilities;
    bool hasDualStreaming = false;
    bool hasAudio = false;
    // TODO: move more fields to here like io port settings e.t.c
};
#define CameraMediaCapability_Fields (streamCapabilities)(hasDualStreaming)(hasAudio)
QN_FUSION_DECLARE_FUNCTIONS(CameraMediaCapability, (json), NX_VMS_COMMON_API)

NX_REFLECTION_ENUM_CLASS(CameraTraitType,
    aspectRatioDependent = 1
)

// TODO: #dmishin it's workaround for fusion bug (impossibility to serialize/deserialize maps
// with the key different from QString). Should be fixed in 4.0
using CameraTraitNameString = QString;

using CameraTraitAttributes = std::map<QString, QString>;
using CameraTraits = std::map<CameraTraitNameString, CameraTraitAttributes>;

} // media
} // nx
