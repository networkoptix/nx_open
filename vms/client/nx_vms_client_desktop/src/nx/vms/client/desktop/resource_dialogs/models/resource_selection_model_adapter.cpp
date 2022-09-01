// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_selection_model_adapter.h"

#include <memory>

#include <QtQml/QtQml>

#include <client/client_settings.h>
#include <core/resource/resource.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/resource_dialogs/models/resource_selection_decorator_model.h>
#include <nx/vms/client/desktop/resource_views/data/camera_extra_status.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/composition_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_entity_builder.h>
#include <nx/vms/client/desktop/resource_views/models/resource_tree_drag_drop_decorator_model.h>
#include <ui/workbench/handlers/workbench_action_handler.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

struct ResourceSelectionModelAdapter::Private
{
    ResourceSelectionModelAdapter* const q;

    std::unique_ptr<entity_item_model::EntityItemModel> resourceTreeModel;
    std::unique_ptr<ResourceSelectionDecoratorModel> selectionDecoratorModel;
    std::unique_ptr<entity_resource_tree::ResourceTreeEntityBuilder> treeEntityBuilder;
    entity_item_model::AbstractEntityPtr rootEntity;

    QnWorkbenchContext* context = nullptr;
    ResourceTree::ResourceFilters resourceTypes = 0;
    ResourceTree::ResourceSelection selectionMode = ResourceTree::ResourceSelection::multiple;
    QString filterText;

    Private(ResourceSelectionModelAdapter* q):
        q(q),
        resourceTreeModel(new entity_item_model::EntityItemModel()),
        selectionDecoratorModel(new ResourceSelectionDecoratorModel())
    {
        selectionDecoratorModel->setSourceModel(resourceTreeModel.get());
        q->setSourceModel(selectionDecoratorModel.get());
        updateSelectionMode();
    }

    void updateRootEntity()
    {
        if (treeEntityBuilder && treeEntityBuilder->user() && resourceTypes != 0)
        {
            auto entity = treeEntityBuilder->createDialogEntities(resourceTypes);
            resourceTreeModel->setRootEntity(entity.get());
            rootEntity = std::move(entity);
        }
        else
        {
            resourceTreeModel->setRootEntity(nullptr);
            rootEntity.reset();
        }
    }

    void updateSelectionMode()
    {
        switch (selectionMode)
        {
            case ResourceTree::ResourceSelection::single:
                selectionDecoratorModel->setSelectionMode(ResourceSelectionMode::SingleSelection);
                break;

            case ResourceTree::ResourceSelection::exclusive:
                selectionDecoratorModel->setSelectionMode(ResourceSelectionMode::ExclusiveSelection);
                break;

            case ResourceTree::ResourceSelection::multiple:
            default:
                selectionDecoratorModel->setSelectionMode(ResourceSelectionMode::MultiSelection);
                break;
        }
    }

    bool isRowAccepted(int sourceRow, const QModelIndex& sourceParent) const
    {
        if (filterText.isEmpty())
            return true;

        return true;
    }
};

ResourceSelectionModelAdapter::ResourceSelectionModelAdapter(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
    setFilter(
        [this](int sourceRow, const QModelIndex& sourceParent)
        {
            return d->isRowAccepted(sourceRow, sourceParent);
        });

    connect(qnSettings, &QnClientSettings::valueChanged, this,
        [this](int id)
        {
            if (id == QnClientSettings::RESOURCE_INFO_LEVEL)
                emit extraInfoRequiredChanged();
        });

    connect(d->selectionDecoratorModel.get(),
        &ResourceSelectionDecoratorModel::selectedResourcesChanged,
        this,
        &ResourceSelectionModelAdapter::selectedResourcesChanged);
}

ResourceSelectionModelAdapter::~ResourceSelectionModelAdapter()
{
    // Required here for forward-declared scoped pointer destruction.
}

QnWorkbenchContext* ResourceSelectionModelAdapter::context() const
{
    return d->context;
}

void ResourceSelectionModelAdapter::setContext(QnWorkbenchContext* context)
{
    if (d->context == context)
        return;

    d->resourceTreeModel->setRootEntity(nullptr);
    d->treeEntityBuilder.reset();
    d->rootEntity.reset();

    d->context = context;
    if (d->context)
    {
        d->treeEntityBuilder.reset(new entity_resource_tree::ResourceTreeEntityBuilder(
            d->context->systemContext()));

        const auto setUser =
            [this](const QnUserResourcePtr& user)
            {
                d->treeEntityBuilder->setUser(user);
                d->updateRootEntity();
            };

        connect(d->context, &QnWorkbenchContext::userChanged, this, setUser);
        setUser(d->context->user());
    }

    emit contextChanged();
}

ResourceTree::ResourceFilters ResourceSelectionModelAdapter::resourceTypes() const
{
    return d->resourceTypes;
}

void ResourceSelectionModelAdapter::setResourceTypes(ResourceTree::ResourceFilters value)
{
    if (d->resourceTypes == value)
        return;

    d->resourceTypes = value;
    d->updateRootEntity();
}

ResourceTree::ResourceSelection ResourceSelectionModelAdapter::selectionMode() const
{
    return d->selectionMode;
}

void ResourceSelectionModelAdapter::setSelectionMode(ResourceTree::ResourceSelection value)
{
    if (d->selectionMode == value)
        return;

    d->selectionMode = value;
    d->updateSelectionMode();

    emit selectionModeChanged();
}

QString ResourceSelectionModelAdapter::filterText() const
{
    return d->filterText;
}

void ResourceSelectionModelAdapter::setFilterText(const QString& value)
{
    const auto trimmedValue = value.trimmed();
    if (d->filterText == trimmedValue)
        return;

    d->filterText = trimmedValue;
    emit filterTextChanged();

    invalidateFilter();
}

bool ResourceSelectionModelAdapter::isExtraInfoRequired() const
{
    return qnSettings->resourceInfoLevel() > Qn::RI_NameOnly;
}

bool ResourceSelectionModelAdapter::isExtraInfoForced(QnResource* resource) const
{
    return resource && resource->hasFlags(Qn::virtual_camera);
}

QSet<QnResourcePtr> ResourceSelectionModelAdapter::selectedResources() const
{
    return d->selectionDecoratorModel->selectedResources();
}

QSet<QnUuid> ResourceSelectionModelAdapter::selectedResourceIds() const
{
    return d->selectionDecoratorModel->selectedResourcesIds();
}

QModelIndex ResourceSelectionModelAdapter::resourceIndex(const QnResourcePtr& resource) const
{
    return mapFromSource(d->selectionDecoratorModel->resourceIndex(resource));
}

QVariant ResourceSelectionModelAdapter::data(const QModelIndex& index, int role) const
{
    if (!d->context || !index.isValid() || !NX_ASSERT(checkIndex(index)))
        return {};

    switch (role)
    {
        case Qn::CameraExtraStatusRole:
            return (int) base_type::data(index, role).value<CameraExtraStatus>();

        case Qn::RawResourceRole:
        {
            const auto resource = data(index, Qn::ResourceRole).value<QnResourcePtr>();
            if (!resource)
                return {};

            QQmlEngine::setObjectOwnership(resource.get(), QQmlEngine::CppOwnership);
            return QVariant::fromValue(resource.get());
        }

        case Qn::AutoExpandRole:
            return true;

        default:
            return base_type::data(index, role);
    }
}

bool ResourceSelectionModelAdapter::setData(
    const QModelIndex& index, const QVariant& value, int role)
{
    if (role != Qt::CheckStateRole)
        return base_type::setData(index, value, role);

    if (data(index, role) == value)
        return false;

    return d->selectionDecoratorModel->toggleSelection(mapToSource(index));
}

Qt::ItemFlags ResourceSelectionModelAdapter::flags(const QModelIndex& index) const
{
    if (!index.isValid() || !NX_ASSERT(checkIndex(index)))
        return Qt::NoItemFlags;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QHash<int, QByteArray> ResourceSelectionModelAdapter::roleNames() const
{
    auto roleNames = base_type::roleNames();
    roleNames[Qn::AutoExpandRole] = "autoExpand";
    roleNames[Qn::RawResourceRole] = "resource";
    return roleNames;
}

void ResourceSelectionModelAdapter::registerQmlType()
{
    qmlRegisterType<ResourceSelectionModelAdapter>(
        "nx.vms.client.desktop", 1, 0, "ResourceSelectionModel");
}

} // namespace nx::vms::client::desktop
