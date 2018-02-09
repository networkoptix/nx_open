#pragma once

#include <common/common_globals.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace media {

struct CameraStreamCapability
{
    int minBitrateKbps = 0;
    int maxBitrateKbps = 0;
    int defaultBitrateKbps = 0;
    int defaultFps = 0;
    int maxFps = 0;

    CameraStreamCapability(int minBitrate = 0, int maxBitrate = 0, int fps = 0);
    bool isNull() const;
    QString toString() const;
};

#define CameraStreamCapability_Fields (minBitrateKbps)(maxBitrateKbps)(defaultBitrateKbps)(defaultFps)(maxFps)

struct CameraMediaCapability
{
    // Todo: It should be a StreamIndex here. But it is unsafe because it is saved to a database and we need
    // compatibility with old DB version for mobile client.
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
