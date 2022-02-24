// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_layout_info_p.h"

#include <QtQml/QQmlEngine>

#include <client/client_runtime_settings.h>
#include <core/resource/layout_resource.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>

namespace nx::vms::client::desktop {

namespace {

template<typename T>
T* withCppOwnership(T* object)
{
    QQmlEngine::setObjectOwnership(object, QQmlEngine::CppOwnership);
    return object;
}

} // namespace

class WorkbenchLayoutInfo::ResourceContainer: public AbstractResourceContainer
{
    WorkbenchLayoutInfo* const q;

public:
    ResourceContainer(WorkbenchLayoutInfo* q):
        AbstractResourceContainer(q),
        q(q)
    {
    }

    virtual bool containsResource(QnResource* resource) const override
    {
        return resource
            && !q->workbench()->currentLayout()->items(resource->toSharedPointer()).empty();
    }
};

WorkbenchLayoutInfo::WorkbenchLayoutInfo(QnWorkbenchContext* context, QObject* parent):
    QObject(parent),
    QnWorkbenchContextAware(context),
    m_resources(new ResourceContainer(this))
{
    connect(workbench(), &QnWorkbench::currentLayoutAboutToBeChanged, this,
        [this]()
        {
             workbench()->currentLayout()->disconnect(this);
             m_controlledVideoWall = {};
        });

    connect(workbench()->currentLayout(), &QnWorkbenchLayout::lockedChanged,
        this, &WorkbenchLayoutInfo::isLockedChanged);

    connect(workbench(), &QnWorkbench::currentLayoutChanged, this,
        [this]()
        {
            connect(workbench()->currentLayout(), &QnWorkbenchLayout::lockedChanged,
                this, &WorkbenchLayoutInfo::isLockedChanged);

            emit currentLayoutChanged();
            emit itemCountChanged();
            emit resourcesChanged();
            emit isLockedChanged();
        });

    connect(workbench(), &QnWorkbench::currentLayoutItemsChanged, this,
        [this]()
        {
            emit itemCountChanged();
            emit resourcesChanged();
        });

    connect(navigator(), &QnWorkbenchNavigator::currentResourceChanged,
        this, &WorkbenchLayoutInfo::currentResourceChanged);
}

QnLayoutResource* WorkbenchLayoutInfo::currentLayout() const
{
    return withCppOwnership(workbench()->currentLayout()->resource().get());
}

QnResource* WorkbenchLayoutInfo::currentResource() const
{
    return withCppOwnership(navigator()->currentResource().get());
}

AbstractResourceContainer* WorkbenchLayoutInfo::resources() const
{
    return m_resources;
}

QnUuid WorkbenchLayoutInfo::reviewedShowreelId() const
{
    return workbench()->currentLayout()->data(Qn::LayoutTourUuidRole).value<QnUuid>();
}

QnVideoWallResource* WorkbenchLayoutInfo::reviewedVideoWall() const
{
    return withCppOwnership(workbench()->currentLayout()->data(Qn::VideoWallResourceRole)
        .value<QnVideoWallResourcePtr>().get());
}

QnVideoWallResource* WorkbenchLayoutInfo::controlledVideoWall() const
{
    if (m_controlledVideoWall.has_value())
        return m_controlledVideoWall->get();

    const auto itemId = controlledVideoWallItemId();
    if (itemId.isNull())
    {
        m_controlledVideoWall = QnVideoWallResourcePtr();
        return nullptr;
    }

    m_controlledVideoWall = resourcePool()->getVideoWallItemByUuid(itemId).videowall();
    QQmlEngine::setObjectOwnership(m_controlledVideoWall->get(), QQmlEngine::CppOwnership);

    return m_controlledVideoWall->get();
}

QnUuid WorkbenchLayoutInfo::controlledVideoWallItemId() const
{
    return workbench()->currentLayout()->data(Qn::VideoWallItemGuidRole).value<QnUuid>();
}

int WorkbenchLayoutInfo::itemCount() const
{
    return workbench()->currentLayout()->items().size();
}

int WorkbenchLayoutInfo::maximumItemCount() const
{
    return qnRuntime->maxSceneItems();
}

bool WorkbenchLayoutInfo::isLocked() const
{
    return workbench()->currentLayout()->locked();
}

bool WorkbenchLayoutInfo::isSearchLayout() const
{
    return workbench()->currentLayout()->isSearchLayout();
}

bool WorkbenchLayoutInfo::isLayoutTourReview() const
{
    return workbench()->currentLayout()->isLayoutTourReview();
}

} // namespace nx::vms::client::desktop
