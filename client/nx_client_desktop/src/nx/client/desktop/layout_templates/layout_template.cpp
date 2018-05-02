#include "layout_template.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace client {
namespace desktop {

const QString LayoutTemplateItem::kResourceItemType = lit("resource");
const QString LayoutTemplateItem::kZoomWindowItemType = lit("zoom_window");
const QString LayoutTemplateItem::kLocalMediaItemType = lit("local_media");
const QString LayoutTemplateItem::kCameraSourceType = lit("camera");
const QString LayoutTemplateItem::kEnhancedCameraSourceType = lit("camera_enhanced");
const QString LayoutTemplateItem::kFileSourceType = lit("file");
const QString LayoutTemplateItem::kItemSourceType = lit("item");

const QString LayoutTemplate::kAnalyticsLayoutType = lit("analytics");

bool LayoutTemplate::isValid() const
{
    return !name.isEmpty() && !type.isEmpty();
}

QList<QString> LayoutTemplate::referencedLocalFiles() const
{
    QSet<QString> result;

    for (const auto& item: block.items)
    {
        if (item.type == LayoutTemplateItem::kResourceItemType
            && item.source.type == LayoutTemplateItem::kFileSourceType)
        {
            result.insert(item.source.value);
        }
    }

    return result.toList();
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (LayoutTemplateItemSource)(LayoutTemplateItem)(LayoutTemplateBlock)(LayoutTemplate),
    (json),
    _Fields,
    (optional, true))

} // namespace desktop
} // namespace client
} // namespace nx
