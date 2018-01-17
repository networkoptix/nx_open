#pragma once

#include <common/common_globals.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace media {

struct CameraStreamCapability
{
    bool isNull() const { return maxFps == 0;  }

    int minBitrateKbps = 0;
    int maxBitrateKbps = 0;
    int defaultBitrateKbps = 0;
    int defaultFps = 0;
    int maxFps = 0;

    QString toString() const;
};

#define CameraStreamCapability_Fields (minBitrateKbps)(maxBitrateKbps)(defaultBitrateKbps)(defaultFps)(maxFps)

struct CameraMediaCapability
{
    bool isNull() const { return streamCapabilities.isEmpty(); }

    QMap<int, CameraStreamCapability> streamCapabilities;
    bool hasDualStreaming = false;
    bool hasAudio = false;
    // TODO: move more fields to here like io port settings e.t.c
};

#define CameraMediaCapability_Fields (streamCapabilities)(hasDualStreaming)(hasAudio)

static const QString kCameraMediaCapabilityParamName = lit("mediaCapabilities");

QN_FUSION_DECLARE_FUNCTIONS(CameraStreamCapability, (json))
QN_FUSION_DECLARE_FUNCTIONS(CameraMediaCapability, (json))

} // media
} // nx
