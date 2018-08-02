#pragma once

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