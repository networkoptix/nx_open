// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_factory.h"

#include <core/resource/layout_resource.h>
#include <nx/utils/log/assert.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>

template<>
nx::vms::client::desktop::ui::workbench::LayoutsFactory*
    Singleton<nx::vms::client::desktop::ui::workbench::LayoutsFactory>::s_instance = nullptr;

namespace nx::vms::client::desktop {
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

QnWorkbenchLayout* LayoutsFactory::create(
    const LayoutResourcePtr& resource,
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
} // namespace nx::vms::client::desktop
