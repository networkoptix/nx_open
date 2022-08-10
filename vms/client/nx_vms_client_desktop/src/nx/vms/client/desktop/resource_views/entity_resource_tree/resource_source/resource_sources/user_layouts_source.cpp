// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_layouts_source.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <client/client_globals.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/user_layout_resource_index.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

UserLayoutsSource::UserLayoutsSource(
    const UserLayoutResourceIndex* sharedLayoutResourceIndex,
    const QnUserResourcePtr& parentUser)
    :
    base_type(),
    m_userLayoutResourceIndex(sharedLayoutResourceIndex),
    m_parentUser(parentUser)
{
    NX_ASSERT(!m_parentUser.isNull());

    connect(m_userLayoutResourceIndex, &UserLayoutResourceIndex::layoutAdded,
        this, &UserLayoutsSource::onLayoutAdded);

    connect(m_userLayoutResourceIndex, &UserLayoutResourceIndex::layoutRemoved,
        this, &UserLayoutsSource::onLayoutRemoved);
}

QVector<QnResourcePtr> UserLayoutsSource::getResources()
{
    const QnUuid parentId = m_parentUser->getId();
    const auto layouts = m_userLayoutResourceIndex->layoutsWithParentUserId(parentId);

    QVector<QnResourcePtr> result;
    for (const auto& layout: layouts)
        processLayout(layout, [&result, layout]() { result.push_back(layout); });

    return result;
}

void UserLayoutsSource::holdLocalLayout(const QnResourcePtr& layoutResource)
{
    m_localLayouts.insert(layoutResource);

    connect(layoutResource.get(), &QnResource::flagsChanged, this,
        [this](const QnResourcePtr& layout)
        {
            if (layout->hasFlags(Qn::remote))
            {
                m_localLayouts.remove(layout);
                layout->disconnect(this);
                onLayoutAdded(layout, layout->getParentId());
            }
        });
}

void UserLayoutsSource::onLayoutAdded(const QnResourcePtr& resource, const QnUuid& parentId)
{
    if (m_parentUser->getId() != parentId)
        return;

    processLayout(resource, [this, resource]() { emit resourceAdded( resource ); });
}

void UserLayoutsSource::onLayoutRemoved(const QnResourcePtr& layout, const QnUuid& parentId)
{
    if (m_localLayouts.contains(layout))
        m_localLayouts.remove(layout);

    if (parentId == m_parentUser->getId())
        emit resourceRemoved(layout);
}

void UserLayoutsSource::processLayout(
    const QnResourcePtr& layoutResource,
    std::function<void()> successHandler)
{
    const auto layout = layoutResource.staticCast<QnLayoutResource>();

    if (layout->data().contains(Qn::LayoutSearchStateRole))
        return;

    if (layout->hasFlags(Qn::local) && !layout->hasFlags(Qn::local_intercom_layout))
    {
        holdLocalLayout(layoutResource);
        return;
    }

    successHandler();
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
