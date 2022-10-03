// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_composer.h"

#include <functional>

#include <client/client_globals.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/flattening_group_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_composition_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_entity_builder.h>
#include <nx/vms/client/desktop/resource_views/resource_tree_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using namespace nx::vms::api;
using namespace entity_item_model;

ResourceTreeComposer::ResourceTreeComposer(
    SystemContext* systemContext,
    ResourceTreeSettings* resourceTreeSettings)
    :
    base_type(nullptr),
    SystemContextAware(systemContext),
    m_resourceTreeSettings(resourceTreeSettings),
    m_entityBuilder(new ResourceTreeEntityBuilder(systemContext))
{
    connect(systemContext->accessController(), &QnWorkbenchAccessController::userChanged,
        this, &ResourceTreeComposer::onWorkbenchUserChanged);

    if (m_resourceTreeSettings)
    {
        connect(m_resourceTreeSettings, &ResourceTreeSettings::showServersInTreeChanged,
            this, &ResourceTreeComposer::onShowServersInTreeChanged);
    }

    onWorkbenchUserChanged(systemContext->accessController()->user());
}

ResourceTreeComposer::~ResourceTreeComposer()
{
    if (m_attachedModel)
        m_attachedModel->setRootEntity(nullptr);
}

void ResourceTreeComposer::onWorkbenchUserChanged(const QnUserResourcePtr& user)
{
    m_entityBuilder->setUser(user);
    rebuildEntity();
}

void ResourceTreeComposer::onShowServersInTreeChanged()
{
    auto composition = static_cast<UniqueKeyCompositionEntity<int>*>(
        m_entityByFilter.at(FilterMode::noFilter).get());

    emit saveExpandedState();
    composition->setSubEntity(devicesGroup, createDevicesEntity());
    emit restoreExpandedState();
}

AbstractEntityPtr ResourceTreeComposer::createDevicesEntity() const
{
    if (!m_resourceTreeSettings)
        return m_entityBuilder->createServersGroupEntity();

    const bool userCanSeeServers =
        accessController()->hasAdminPermissions()
            || systemSettings()->showServersInTreeForNonAdmins();

    const bool showServers = userCanSeeServers
        && m_resourceTreeSettings->showServersInTree();

    return showServers
        ? m_entityBuilder->createServersGroupEntity()
        : m_entityBuilder->createCamerasAndDevicesGroupEntity();
}

void ResourceTreeComposer::rebuildEntity()
{
    const auto currentUser = accessController()->user();
    const bool isAdmin = accessController()->hasAdminPermissions();

    if (m_attachedModel)
        m_attachedModel->setRootEntity(nullptr);

    auto composition = std::make_unique<UniqueKeyCompositionEntity<int>>();

    if (!currentUser.isNull())
    {
        composition->setSubEntity(currentSystemItem, m_entityBuilder->createCurrentSystemEntity());
        composition->setSubEntity(currentUserItem, m_entityBuilder->createCurrentUserEntity());
        composition->setSubEntity(separatorItem, m_entityBuilder->createSeparatorEntity());

        composition->setSubEntity(devicesGroup, createDevicesEntity());
        composition->setSubEntity(layoutsGroup, m_entityBuilder->createLayoutsGroupEntity());
        composition->setSubEntity(showreelsGroup, m_entityBuilder->createShowreelsGroupEntity());
        composition->setSubEntity(videowallsList, m_entityBuilder->createVideowallsEntity());
        composition->setSubEntity(webPagesGroup, m_entityBuilder->createWebPagesGroupEntity());

        if (isAdmin)
            composition->setSubEntity(usersGroup, m_entityBuilder->createUsersGroupEntity());

        composition->setSubEntity(
            otherSystemsGroup, m_entityBuilder->createOtherSystemsGroupEntity());

        composition->setSubEntity(
            localResourcesGroup, m_entityBuilder->createLocalFilesGroupEntity());

        m_entityByFilter[FilterMode::allServersFilter] = m_entityBuilder->createAllServersEntity();
        m_entityByFilter[FilterMode::allDevicesFilter] = m_entityBuilder->createAllCamerasEntity();
        m_entityByFilter[FilterMode::allLayoutsFilter] = m_entityBuilder->createAllLayoutsEntity();
    }
    else
    {
        auto localFilesEntity = m_entityBuilder->createLocalFilesGroupEntity();
        if (const auto group = dynamic_cast<FlatteningGroupEntity*>(localFilesEntity.get()))
            group->setFlattened(true);

        composition->setSubEntity(localResourcesGroup, std::move(localFilesEntity));
    }

    m_entityByFilter[FilterMode::noFilter] = std::move(composition);
    if (m_attachedModel)
        m_attachedModel->setRootEntity(entityForCurrentFilterMode());
}

AbstractEntity* ResourceTreeComposer::entityForCurrentFilterMode() const
{
    if (!NX_ASSERT(m_entityByFilter.contains(m_filterMode)))
        return nullptr;

    return m_entityByFilter.at(m_filterMode).get();
}

void ResourceTreeComposer::attachModel(EntityItemModel* model)
{
    if (m_attachedModel == model)
        return;

    if (m_attachedModel)
        m_attachedModel->setRootEntity(nullptr);

    m_attachedModel = model;

    if (m_attachedModel)
        m_attachedModel->setRootEntity(entityForCurrentFilterMode());
}

void ResourceTreeComposer::setFilterMode(FilterMode filterMode)
{
    if (filterMode == m_filterMode)
        return;

    m_filterMode = filterMode;

    if (!m_attachedModel)
        return;

    m_attachedModel->setRootEntity(entityForCurrentFilterMode());
}

ResourceTreeComposer::FilterMode ResourceTreeComposer::filterMode() const
{
    return m_filterMode;
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
