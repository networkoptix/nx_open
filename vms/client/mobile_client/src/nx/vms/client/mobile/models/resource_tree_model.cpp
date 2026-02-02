// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_model.h"

#include <core/resource/resource.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/common/models/item_model_algorithm.h>
#include <nx/vms/client/core/qml/qml_ownership.h>
#include <nx/vms/client/core/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/mobile/resource_views/entity_resource_tree/resource_tree_entity_builder.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/client/mobile/window_context.h>

namespace nx::vms::client::mobile {

struct ResourceTreeModel::Private
{
    ResourceTreeModel* q;
    std::unique_ptr<entity_resource_tree::ResourceTreeEntityBuilder> treeBuilder;
    std::unique_ptr<core::entity_item_model::AbstractEntity> rootEntity;
    std::unique_ptr<core::entity_item_model::EntityItemModel> baseTreeModel;
};

ResourceTreeModel::ResourceTreeModel(QObject* parent):
    base_type(parent),
    WindowContextAware(WindowContext::fromQmlContext(this)),
    d(new Private{this})
{
    d->baseTreeModel = std::make_unique<core::entity_item_model::EntityItemModel>();
    setSourceModel(d->baseTreeModel.get());
}

ResourceTreeModel::~ResourceTreeModel()
{
    d->baseTreeModel->setRootEntity({});
    setSourceModel({});
}

QHash<int, QByteArray> ResourceTreeModel::roleNames() const
{
    auto roles = base_type::roleNames();
    roles.insert(core::RawResourceRole, "resource");
    return roles;
}

QVariant ResourceTreeModel::data(const QModelIndex& index, int role) const
{
    if (role == core::RawResourceRole)
    {
        if (const auto resource = data(index, core::ResourceRole).value<QnResourcePtr>())
            return QVariant::fromValue(core::withCppOwnership(resource.get()));
    }
    return base_type::data(index, role);
}

QModelIndex ResourceTreeModel::resourceIndex(QnResource* resource) const
{
    if (!resource)
        return {};

    const auto leafIndexes = core::item_model::getLeafIndexes(this);
    for (const auto& index: leafIndexes)
    {
        const auto indexResource = data(index, core::ResourceRole).value<QnResourcePtr>();
        if (indexResource && indexResource->getId() == resource->getId())
            return index;
    }
    return {};
}

void ResourceTreeModel::onContextReady()
{
    d->treeBuilder = std::make_unique<entity_resource_tree::ResourceTreeEntityBuilder>(
        windowContext()->mainSystemContext());

    const auto updateRootEntity =
        [this]()
        {
            d->baseTreeModel->setRootEntity({});
            d->rootEntity.reset();
            if (const auto user = windowContext()->mainSystemContext()->user())
            {
                d->treeBuilder->setUser(user);
                d->rootEntity = d->treeBuilder->createTreeEntity();
                d->baseTreeModel->setRootEntity(d->rootEntity.get());

                emit rootEntityUpdated();
            }
        };

    updateRootEntity();

    connect(windowContext()->mainSystemContext(), &SystemContext::userChanged,
        this, updateRootEntity);
}

} // namespace nx::vms::client::mobile
