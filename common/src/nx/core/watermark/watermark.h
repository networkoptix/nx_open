#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <utils/common/watermark_settings.h>

namespace nx {
namespace core {

struct Watermark
{
    QnWatermarkSettings settings;
    QString text;

    bool visible() const { return settings.useWatermark && !text.isEmpty(); };
};

} // namespace core
} // namespace nx

#define Watermark_Fields (settings)(text)

QN_FUSION_DECLARE_FUNCTIONS(nx::core::Watermark, (json))

Q_DECLARE_METATYPE(nx::core::Watermark)
