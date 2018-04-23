#pragma once

#include <vector>

#include <common/common_globals.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/core/utils/attribute.h>

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
    QMap<Qn::StreamIndex, CameraStreamCapability> streamCapabilities;
    bool hasDualStreaming = false;
    bool hasAudio = false;
    // TODO: move more fields to here like io port settings e.t.c
};

#define CameraMediaCapability_Fields (streamCapabilities)(hasDualStreaming)(hasAudio)

static const QString kCameraMediaCapabilityParamName = lit("mediaCapabilities");

QN_FUSION_DECLARE_FUNCTIONS(CameraStreamCapability, (json))
QN_FUSION_DECLARE_FUNCTIONS(CameraMediaCapability, (json))

enum class CameraStreamCapabilityTraitType
{
    aspectRatioDependent = 1
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(CameraStreamCapabilityTraitType);

struct CameraStreamCapabilityTrait
{
    CameraStreamCapabilityTraitType trait;
    nx::core::utils::AttributeList attributes;
};

using CameraStreamCapabilityTraits = std::vector<CameraStreamCapabilityTrait>;

#define CameraStreamCapabilityTrait_Fields (trait)(attributes)
QN_FUSION_DECLARE_FUNCTIONS(CameraStreamCapabilityTrait, (json)(metatype));

} // media
} // nx

QN_FUSION_DECLARE_FUNCTIONS(nx::media::CameraStreamCapabilityTraitType, (lexical));

