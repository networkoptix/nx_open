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
#define Watermark_Fields (settings)(text)

QN_FUSION_DECLARE_FUNCTIONS(Watermark, (json)(eq))

} // namespace core
} // namespace nx

Q_DECLARE_METATYPE(nx::core::Watermark)
