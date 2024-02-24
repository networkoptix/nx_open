// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <unordered_map>

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/abstract_entity.h>
#include <nx/vms/client/desktop/system_context_aware.h>

class QnWorkbenchContext;

namespace nx::vms::client::desktop::entity_item_model { class EntityItemModel; }

namespace nx::vms::client::desktop {

class ResourceTreeSettings;

namespace entity_resource_tree {

class ResourceTreeEntityBuilder;

class NX_VMS_CLIENT_DESKTOP_API ResourceTreeComposer:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    ResourceTreeComposer(
        SystemContext* systemContext,
        ResourceTreeSettings* resourceTreeSettings);

    virtual ~ResourceTreeComposer() override;

    enum class FilterMode
    {
        noFilter,
        allServersFilter,
        allDevicesFilter,
        allLayoutsFilter,
    };

    /**
     * @param model EntityItemModel instance which will be facade for one of encapsulated entities,
     *     depending on current filter mode applied. Composer doesn't take ownership of model, and
     *     there is no constraints regarding model lifetime.
     */
    void attachModel(entity_item_model::EntityItemModel* model);

    FilterMode filterMode() const;
    void setFilterMode(FilterMode filterMode);

signals:
    void saveExpandedState();
    void restoreExpandedState();

private:
    /** Corresponding entities will be arranged in the enumeration definitions order. */
    enum TopLevelEntityKey
    {
        currentSystemItem,
        currentUserItem,
        separatorItem,
        devicesGroup,
        layoutsGroup,
        showreelsGroup,
        videowallsList,
        integrationsGroup,
        webPagesGroup,
        usersGroup,
        otherSystemsGroup,
        localResourcesGroup
    };

    void onUserChanged();
    void onTreeSettingsChanged();
    entity_item_model::AbstractEntityPtr createDevicesEntity() const;
    void rebuildEntity();
    entity_item_model::AbstractEntity* entityForCurrentFilterMode() const;

private:
    const ResourceTreeSettings* m_resourceTreeSettings;

    QScopedPointer<ResourceTreeEntityBuilder> m_entityBuilder;
    std::unordered_map<FilterMode, entity_item_model::AbstractEntityPtr> m_entityByFilter;
    QPointer<entity_item_model::EntityItemModel> m_attachedModel;

    FilterMode m_filterMode = FilterMode::noFilter;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
