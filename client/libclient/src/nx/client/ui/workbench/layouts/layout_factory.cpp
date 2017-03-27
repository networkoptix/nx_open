#include "layout_factory.h"

#include <core/resource/layout_resource.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace ui {
namespace workbench {
namespace layouts {

LayoutsFactory::LayoutsFactory(QObject* parent):
    base_type(parent)
{
}

QnWorkbenchLayout* LayoutsFactory::create(QObject* parent)
{
    return create(QnLayoutResourcePtr(), parent);
}

QnWorkbenchLayout* LayoutsFactory::create(
    const QnLayoutResourcePtr& resource,
    QObject* parent)
{
    for (const auto& creator: m_creators)
    {
        if (const auto layout = creator(resource, parent))
            return layout;
    }

    NX_ASSERT(false, "No creators found");
    return nullptr;
}

void LayoutsFactory::addCreator(const LayoutCreator& creator)
{
    m_creators.prepend(creator);
}

} // namespace layouts
} // namespace workbench
} // namespace ui
} // namespace client
} // namespace nx
