#include "layout_factory.h"

#include <core/resource/layout_resource.h>
#include <nx/utils/log/assert.h>

namespace {

int toInt(Qn::LayoutResourceType type)
{
    return static_cast<int>(type);
}

} // namespace

namespace nx {
namespace client {
namespace ui {
namespace workbench {
namespace layouts {

LayoutsFactory::LayoutsFactory(
    const LayoutCreator& emptyResourceLayoutCreator,
    QObject* parent)
    :
    base_type(parent),
    m_emptyResourceLayoutCreator(emptyResourceLayoutCreator)
{
}

QnWorkbenchLayout* LayoutsFactory::create(QObject* parent)
{
    if (m_emptyResourceLayoutCreator)
        return m_emptyResourceLayoutCreator(QnLayoutResourcePtr(), parent);

    NX_ASSERT(false, "No default creator for empty layout resource");
    return nullptr;
}

QnWorkbenchLayout* LayoutsFactory::create(
    const QnLayoutResourcePtr& resource,
    QObject* parent)
{
    if (!resource)
    {
        NX_ASSERT(false, "No empty resource is allowed in layout creation");
        return create(parent);
    }

    const auto itCreator = m_creators.find(toInt(resource->type()));
    if (itCreator == m_creators.end())
    {
        NX_ASSERT(false, "Can't find creator for specifiedd layout type");
        return nullptr;
    }

    const auto& creator = itCreator.value();
    return creator(resource, parent);
}

void LayoutsFactory::addCreator(
    Qn::LayoutResourceType type,
    const LayoutCreator& creator)
{
    const int intType = toInt(type);
    if (m_creators.contains(intType))
    {
        NX_ASSERT(false, "Creator for specified layout type exists already");
        return;
    }

    m_creators.insert(intType, creator);
}

} // namespace layouts
} // namespace workbench
} // namespace ui
} // namespace client
} // namespace nx
