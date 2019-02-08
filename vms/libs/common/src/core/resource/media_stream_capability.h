#pragma once

#include <vector>

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
QN_FUSION_DECLARE_FUNCTIONS(CameraStreamCapability, (json))

struct CameraMediaCapability
{
    QMap<nx::vms::api::MotionStreamType, CameraStreamCapability> streamCapabilities;
    bool hasDualStreaming = false;
    bool hasAudio = false;
    // TODO: move more fields to here like io port settings e.t.c
};
#define CameraMediaCapability_Fields (streamCapabilities)(hasDualStreaming)(hasAudio)
QN_FUSION_DECLARE_FUNCTIONS(CameraMediaCapability, (json))

enum class CameraTraitType
{
    aspectRatioDependent = 1
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(CameraTraitType);

// TODO: #dmishin it's workaround for fusion bug (impossibility to serialize/deserialize maps
// with the key different from QString). Should be fixed in 4.0
using CameraTraitNameString = QString;

using CameraTraitAttributes = std::map<QString, QString>;
using CameraTraits = std::map<CameraTraitNameString, CameraTraitAttributes>;

} // media
} // nx

Q_DECLARE_METATYPE(nx::media::CameraTraits)
QN_FUSION_DECLARE_FUNCTIONS(nx::media::CameraTraitType, (lexical));
