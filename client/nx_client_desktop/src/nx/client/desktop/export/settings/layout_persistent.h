#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/client/desktop/export/data/export_layout_settings.h>

namespace nx {
namespace client {
namespace desktop {
namespace settings {

struct ExportLayoutPersistent: public ExportLayoutSettings
{
};
#define ExportLayoutPersistent_Fields (readOnly)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((ExportLayoutPersistent), (metatype)(json))

} // namespace settings
} // namespace desktop
} // namespace client
} // namespace nx
