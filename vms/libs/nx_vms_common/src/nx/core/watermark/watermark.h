// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/vms/api/data/watermark_settings.h>

namespace nx {
namespace core {

struct Watermark
{
    vms::api::WatermarkSettings settings;
    QString text;

    bool operator==(const Watermark& other) const = default;

    bool visible() const { return settings.useWatermark && !text.isEmpty(); };
};
#define Watermark_Fields (settings)(text)

QN_FUSION_DECLARE_FUNCTIONS(Watermark, (json), NX_VMS_COMMON_API)

} // namespace core
} // namespace nx

Q_DECLARE_METATYPE(nx::core::Watermark)
