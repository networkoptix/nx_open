#include "layout_factory.h"

#include <core/resource/layout_resource.h>
#include <nx/utils/log/assert.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

LayoutsFactory* LayoutsFactory::instance(QnWorkbench* workbench)
{
    return workbench->context()->instance<LayoutsFactory>();
}

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

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
