// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_composer.h"

#include <functional>

#include <client/client_globals.h>
#include <core/resource_access/resource_access_subject.h>
#include <nx/vms/client/desktop/access/caching_access_controller.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/flattening_group_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_composition_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_entity_builder.h>
#include <nx/vms/client/desktop/resource_views/resource_tree_settings.h>
#include <nx/vms/common/system_settings.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using namespace nx::vms::api;
using namespace entity_item_model;

using nx::vms::common::SystemSettings;

ResourceTreeComposer::ResourceTreeComposer(
    SystemContext* systemContext,
    ResourceTreeSettings* resourceTreeSettings)
    :
    base_type(nullptr),
    SystemContextAware(systemContext),
    m_resourceTreeSettings(resourceTreeSettings),
    m_entityBuilder(new ResourceTreeEntityBuilder(systemContext))
{
    connect(accessController(), &AccessController::userChanged,
        this, &ResourceTreeComposer::onUserChanged);

    connect(systemSettings(), &SystemSettings::showServersInTreeForNonAdminsChanged,
        this, &ResourceTreeComposer::onTreeSettingsChanged);

    if (m_resourceTreeSettings)
    {
        connect(m_resourceTreeSettings, &ResourceTreeSettings::showServersInTreeChanged,
            this, &ResourceTreeComposer::onTreeSettingsChanged);

        connect(
            m_resourceTreeSettings,
            &ResourceTreeSettings::showProxiedResourcesInServerTreeChanged,
            this,
            &ResourceTreeComposer::onTreeSettingsChanged);
    }

    onUserChanged();
}

ResourceTreeComposer::~ResourceTreeComposer()
{
    if (m_attachedModel)
        m_attachedModel->setRootEntity(nullptr);
}

void ResourceTreeComposer::onUserChanged()
{
    m_entityBuilder->setUser(accessController()->user());
    rebuildEntity();
}

void ResourceTreeComposer::onTreeSettingsChanged()
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
        return m_entityBuilder->createServersGroupEntity(/*showProxiedResources*/ false);

    const bool userCanSeeServers = accessController()->hasPowerUserPermissions()
        || systemSettings()->showServersInTreeForNonAdmins();

    const bool showServers = userCanSeeServers
        && m_resourceTreeSettings->showServersInTree();

    const bool showProxiedResources = m_resourceTreeSettings->showProxiedResourcesInServerTree();

    return showServers
        ? m_entityBuilder->createServersGroupEntity(showProxiedResources)
        : m_entityBuilder->createCamerasAndDevicesGroupEntity(showProxiedResources);
}

void ResourceTreeComposer::rebuildEntity()
{
    const auto currentUser = accessController()->user();
    const bool isAdmin = accessController()->hasPowerUserPermissions();

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

        if (ini().webPagesAndIntegrations)
        {
            composition->setSubEntity(
                integrationsGroup, m_entityBuilder->createIntegrationsGroupEntity());
        }

        composition->setSubEntity(webPagesGroup, m_entityBuilder->createWebPagesGroupEntity());

        if (!ini().hideOtherSystemsFromResourceTree)
        {
            composition->setSubEntity(
                otherSystemsGroup, m_entityBuilder->createOtherSystemsGroupEntity());
        }

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
