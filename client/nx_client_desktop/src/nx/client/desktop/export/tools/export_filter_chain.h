#pragma once

#include <transcoding/filters/abstract_image_filter.h>

namespace nx {
namespace client {
namespace desktop {

struct ExportMediaSettings;

class ExportFilterChain
{
    static QnAbstractImageFilterList construct(const ExportMediaSettings& settings);
};

} // namespace desktop
} // namespace client
} // namespace nx
