// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_model_adapter.h"

#include <memory>

#include <QtCore/QVector>
#include <QtCore/QStack>
#include <QtQml/QtQml>

#include <client/client_globals.h>
#include <client/client_settings.h>
#include <common/common_globals.h>
#include <common/common_module.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_resource.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/common/models/item_model_algorithm.h>
#include <nx/vms/client/desktop/resource_views/data/camera_extra_status.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_composer.h>
#include <nx/vms/client/desktop/resource_views/models/resource_tree_drag_drop_decorator_model.h>
#include <nx/vms/client/desktop/resource_views/resource_tree_edit_delegate.h>
#include <nx/vms/client/desktop/resource_views/resource_tree_interaction_handler.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/scene/models/resource_tree_squish_facade.h>
#include <nx/vms/client/desktop/utils/virtual_camera_manager.h>
#include <nx/vms/client/desktop/utils/virtual_camera_state.h>
#include <nx/vms/common/system_settings.h>
#include <ui/models/resource_search_proxy_model.h>
#include <ui/workbench/handlers/workbench_action_handler.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr int kMaxAutoExpandedServers = 2;

ResourceTree::NodeType filterNodeType(ResourceTree::FilterType filterType)
{
    using ResourceTree::NodeType;
    using ResourceTree::FilterType;

    switch (filterType)
    {
        case FilterType::noFilter:
        case FilterType::everything:
            return NodeType::root;

        case FilterType::servers:
            return NodeType::servers;

        case FilterType::cameras:
            return NodeType::camerasAndDevices;

        case FilterType::layouts:
            return NodeType::layouts;

        case FilterType::showreels:
            return NodeType::layoutTours;

        case FilterType::videowalls:
            return NodeType::videoWalls;

        case FilterType::webPages:
            return NodeType::webPages;

        case FilterType::users:
            return NodeType::users;

        case FilterType::localFiles:
            return NodeType::localResources;
    }

    NX_ASSERT(false);
    return {};
}

bool isTopLevelNodeType(ResourceTree::NodeType nodeType)
{
    return nodeType > ResourceTree::NodeType::separator
        && nodeType <= ResourceTree::NodeType::localResources;
}

} // namespace

struct ResourceTreeModelAdapter::Private
{
    ResourceTreeModelAdapter* const q;
    using FilterType = ResourceTree::FilterType;

    std::unique_ptr<entity_item_model::EntityItemModel> resourceTreeModel;
    std::unique_ptr<entity_resource_tree::ResourceTreeComposer> resourceTreeComposer;
    std::unique_ptr<ResourceTreeDragDropDecoratorModel> dragDropDecoratorModel;
    std::unique_ptr<QnResourceSearchProxyModel> sortFilterModel;
    std::unique_ptr<ResourceTreeInteractionHandler> interactionHandler;
    QStack<QVector<QPersistentModelIndex>> states;

    QnWorkbenchContext* context = nullptr;
    bool localFilesMode = true;

    FilterType filterType = FilterType::noFilter;
    QString filterText;
    bool isFiltering = false;
    QPersistentModelIndex rootIndex;

    QVector<ResourceTree::ShortcutHint> shortcutHints;

    bool defaultAutoExpandedNodes = true;
    QSet<ResourceTree::ExpandedNodeId> autoExpandedNodes;

    ResourceTreeModelSquishFacade* squishFacade = nullptr;

    using NodeType = ResourceTree::NodeType;

    Private(ResourceTreeModelAdapter* q):
        q(q),
        resourceTreeModel(new entity_item_model::EntityItemModel()),
        sortFilterModel(new QnResourceSearchProxyModel(q)),
        squishFacade(new ResourceTreeModelSquishFacade(q))
    {
        q->setSourceModel(sortFilterModel.get());
    }

    FilterType effectiveFilterType() const
    {
        return localFilesMode ? FilterType::localFiles : filterType;
    }

    void updateFilter()
    {
        if (!NX_ASSERT(resourceTreeComposer))
            return;

        const auto rootNodeType = filterNodeType(effectiveFilterType());

        using FilterMode = entity_resource_tree::ResourceTreeComposer::FilterMode;
        switch (rootNodeType)
        {
            case NodeType::servers:
                resourceTreeComposer->setFilterMode(FilterMode::allServersFilter);
                break;
            case NodeType::camerasAndDevices:
                resourceTreeComposer->setFilterMode(FilterMode::allDevicesFilter);
                break;
            case NodeType::layouts:
                resourceTreeComposer->setFilterMode(FilterMode::allLayoutsFilter);
                break;

            default:
                resourceTreeComposer->setFilterMode(FilterMode::noFilter);
                break;
        }

        const auto newRootIndex = q->mapFromSource(
            sortFilterModel->setQuery(QnResourceSearchQuery(filterText, rootNodeType)));

        const bool oldIsFiltering = isFiltering;
        isFiltering = !filterText.isEmpty() || filterType != FilterType::noFilter;

        if (oldIsFiltering != isFiltering)
            emit q->isFilteringChanged();

        if (newRootIndex != rootIndex)
        {
            rootIndex = newRootIndex;
            emit q->rootIndexChanged();
        }

        updateShortcutHints();
    }

    void updateLocalFilesMode()
    {
        const bool newLocalFilesMode = !context || !context->user();
        if (localFilesMode == newLocalFilesMode)
            return;

        localFilesMode = newLocalFilesMode;
        emit q->localFilesModeChanged();

        updateFilter();
    }

    void updateShortcutHints()
    {
        const auto newShortcutHints = calculateShortcutHints();
        if (shortcutHints == newShortcutHints)
            return;

        shortcutHints = newShortcutHints;
        emit q->shortcutHintsChanged();
    }

    QVector<ResourceTree::ShortcutHint> calculateShortcutHints() const
    {
        if (!isFiltering || q->rowCount(rootIndex) == 0)
            return {};

        static const QVector<ResourceTree::ShortcutHint> kResourceHints({
            {{Qt::Key_Enter}, tr("add to current layout")},
            {{Qt::Key_Control, Qt::Key_Enter}, tr("open all at a new layout")}});

        static const QVector<ResourceTree::ShortcutHint> kLayoutHints({
            {{Qt::Key_Control, Qt::Key_Enter}, tr("open all")}});

        switch (effectiveFilterType())
        {
            case FilterType::servers:
            case FilterType::users:
                return {};

            case FilterType::cameras:
            case FilterType::webPages:
                return kResourceHints;

            case FilterType::layouts:
            case FilterType::showreels:
            case FilterType::videowalls:
                return kLayoutHints;

            case FilterType::noFilter:
            case FilterType::everything:
            case FilterType::localFiles:
            {
                bool hasResources = false;
                bool hasLayoutEntities = false; //< Has entities openable as workbench layouts.
                bool hasNotOpenableItems = false;

                const auto indexes = item_model::getAllIndexes(q, rootIndex);
                for (const auto& index: indexes)
                {
                    if ((hasResources && hasLayoutEntities) || hasNotOpenableItems)
                        break;

                    if (index.data(Qn::NodeTypeRole).value<ResourceTree::NodeType>()
                        == ResourceTree::NodeType::layoutTour)
                    {
                        hasLayoutEntities = true;
                        continue;
                    }

                    const auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
                    if (!resource)
                    {
                        hasNotOpenableItems = hasNotOpenableItems || q->rowCount(index) == 0;
                        continue;
                    }

                    if (!QnResourceAccessFilter::isDroppable(resource))
                    {
                        hasNotOpenableItems = true;
                        continue;
                    }

                    if (const auto videowall = resource.objectCast<QnVideoWallResource>())
                        hasLayoutEntities = true;
                    else if (const auto layout = resource.objectCast<QnLayoutResource>())
                        hasLayoutEntities = true;
                    else
                        hasResources = true;
                }

                if (hasResources == hasLayoutEntities || hasNotOpenableItems)
                    return {};

                return hasResources ? kResourceHints : kLayoutHints;
            }
        }

        return {};
    }
};

// QSortFilterProxyModel in its constructor connects to its own modelReset to update data mapping.
// However, QML signal receivers are invoked before C++ receivers.
// It causes QML code to work incorrectly in onModelReset handlers.
// To overcome this trouble, another proxying layer - QIdentityProxyModel - is added to the chain.

ResourceTreeModelAdapter::ResourceTreeModelAdapter(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
    connect(qnSettings, &QnClientSettings::valueChanged, this,
        [this](int id)
        {
            if (id == QnClientSettings::RESOURCE_INFO_LEVEL)
                emit extraInfoRequiredChanged();
        });
}

ResourceTreeModelAdapter::~ResourceTreeModelAdapter()
{
    // Required here for forward-declared scoped pointer destruction.
}

QnWorkbenchContext* ResourceTreeModelAdapter::context() const
{
    return d->context;
}

void ResourceTreeModelAdapter::setContext(QnWorkbenchContext* context)
{
    if (d->context == context)
        return;

    d->sortFilterModel->setSourceModel(nullptr);

    d->context = context;
    if (d->context)
    {
        d->resourceTreeComposer = std::make_unique<entity_resource_tree::ResourceTreeComposer>(
            d->context->systemContext(),
            d->context->resourceTreeSettings());

        connect(d->resourceTreeComposer.get(), &entity_resource_tree::ResourceTreeComposer::saveExpandedState,
            this, &ResourceTreeModelAdapter::saveExpandedState);

        connect(d->resourceTreeComposer.get(), &entity_resource_tree::ResourceTreeComposer::restoreExpandedState,
            this, &ResourceTreeModelAdapter::restoreExpandedState);

        d->dragDropDecoratorModel.reset(new ResourceTreeDragDropDecoratorModel(
            d->context->resourcePool(),
            d->context->menu(),
            d->context->instance<ui::workbench::ActionHandler>()));

        d->dragDropDecoratorModel->setSourceModel(d->resourceTreeModel.get());

        d->sortFilterModel->setSourceModel(d->dragDropDecoratorModel.get());

        d->resourceTreeComposer->attachModel(d->resourceTreeModel.get());

        d->resourceTreeModel->setEditDelegate(ResourceTreeEditDelegate(context));

        connect(d->context, &QnWorkbenchContext::userChanged, this,
            [this]() { d->updateLocalFilesMode(); });

        d->interactionHandler.reset(new ResourceTreeInteractionHandler(d->context));

        connect(d->interactionHandler.get(), &ResourceTreeInteractionHandler::editRequested,
            this, &ResourceTreeModelAdapter::editRequested);
    }

    emit contextChanged();

    d->updateLocalFilesMode();
    d->updateFilter();
}

QVariant ResourceTreeModelAdapter::data(const QModelIndex& index, int role) const
{
    if (!d->context)
        return {};

    switch (role)
    {
        case Qn::ExtraInfoRole:
        {
            static const QString kCustomExtInfoTemplate = //< "- %1" with en-dash
                QString::fromWCharArray(L"\x2013 %1");

            const auto resource = base_type::data(index, Qn::ResourceRole).value<QnResourcePtr>();
            const auto resourceFlags = resource ? resource->flags() : Qn::ResourceFlags{};
            const auto nodeType = base_type::data(
                index, Qn::NodeTypeRole).value<ResourceTree::NodeType>();

            if ((nodeType == ResourceTree::NodeType::layoutItem
                || nodeType == ResourceTree::NodeType::sharedResource)
                && resourceFlags.testFlag(Qn::server) && !resourceFlags.testFlag(Qn::fake))
            {
                return kCustomExtInfoTemplate.arg(tr("Health Monitor"));
            }

            const auto extraInfo = base_type::data(index, role).toString();
            if (extraInfo.isEmpty())
                return {};

            if (resourceFlags.testFlag(Qn::user))
                return kCustomExtInfoTemplate.arg(extraInfo);

            if (resourceFlags.testFlag(Qn::virtual_camera))
            {
                const auto camera = resource.dynamicCast<QnSecurityCamResource>();
                auto systemContext = SystemContext::fromResource(camera);
                const auto state = systemContext->virtualCameraManager()->state(camera);
                if (state.isRunning())
                    return kCustomExtInfoTemplate.arg(QString::number(state.progress()) + lit("%"));
            }

            return extraInfo;
        }

        case Qn::CameraExtraStatusRole:
            return int(base_type::data(index, Qn::CameraExtraStatusRole).value<CameraExtraStatus>());

        case Qn::RawResourceRole:
        {
            const auto resource = data(index, Qn::ResourceRole).value<QnResourcePtr>();
            if (!resource)
                return {};

            QQmlEngine::setObjectOwnership(resource.get(), QQmlEngine::CppOwnership);
            return QVariant::fromValue(resource.get());
        }

        case Qn::AutoExpandRole:
        {
            if (d->isFiltering && hasChildren(index))
                return true;

            if (!d->defaultAutoExpandedNodes)
            {
                const auto nodeId = expandedNodeId(index);
                return nodeId && d->autoExpandedNodes.contains(nodeId);
            }

            using ResourceTree::NodeType;
            switch (index.data(Qn::NodeTypeRole).value<NodeType>())
            {
                case NodeType::resource:
                {
                    const auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
                    if (resource && resource->hasFlags(Qn::server))
                    {
                        const auto serverCount = d->context->resourcePool()
                            ->getResources<QnMediaServerResource>().count();

                        return serverCount <= kMaxAutoExpandedServers;
                    }

                    return false;
                }

                case NodeType::servers:
                case NodeType::camerasAndDevices:
                    return true;

                default:
                    return false;
            }
        }

        default:
            return base_type::data(index, role);
    }
}

QHash<int, QByteArray> ResourceTreeModelAdapter::roleNames() const
{
    auto roleNames = d->sortFilterModel->roleNames();
    roleNames[Qn::AutoExpandRole] = "autoExpand";
    roleNames[Qn::RawResourceRole] = "resource";
    roleNames[Qn::HelpTopicIdRole] = "helpTopicId";
    roleNames[Qn::UuidRole] = "uuid";
    roleNames[Qn::ResourceTreeCustomGroupIdRole] = "customGroupId";
    roleNames[Qn::CameraGroupIdRole] = "cameraGroupId";
    return roleNames;
}

bool ResourceTreeModelAdapter::isFilterRelevant(ResourceTree::FilterType type) const
{
    if (!d->context)
        return true;

    switch (type)
    {
        case ResourceTree::FilterType::servers:
            return d->context->accessController()->hasGlobalPermission(GlobalPermission::admin)
                || d->context->systemSettings()->showServersInTreeForNonAdmins();

        default:
            return true;
    }
}

void ResourceTreeModelAdapter::pushState(const QModelIndexList& state)
{
    if (!NX_ASSERT(d->resourceTreeModel && d->dragDropDecoratorModel && d->sortFilterModel))
        return;

    QVector<QPersistentModelIndex> persistentState;
    persistentState.reserve(state.size());

    for (const auto& index: state)
    {
        if (!index.isValid() || !NX_ASSERT(checkIndex(index)))
            continue;

        const auto sourceIndex =
            d->dragDropDecoratorModel->mapToSource(
                d->sortFilterModel->mapToSource(
                    mapToSource(index)));

        if (sourceIndex.isValid() && NX_ASSERT(sourceIndex.model() == d->resourceTreeModel.get()))
            persistentState.push_back(sourceIndex);
    }

    d->states.push(persistentState);

    NX_VERBOSE(this, "Pushed state, %1 indices, new stack depth: %2",
        persistentState.size(), d->states.size());
}

QModelIndexList ResourceTreeModelAdapter::popState()
{
    if (!NX_ASSERT(!d->states.empty()
        && d->resourceTreeModel && d->dragDropDecoratorModel && d->sortFilterModel))
    {
        return {};
    }

    QModelIndexList state;
    for (const auto& sourceIndex: d->states.pop())
    {
        if (!sourceIndex.isValid() || !NX_ASSERT(d->resourceTreeModel->checkIndex(sourceIndex)))
            continue;

        const auto index = mapFromSource(
            d->sortFilterModel->mapFromSource(
                d->dragDropDecoratorModel->mapFromSource(sourceIndex)));

        if (index.isValid() && NX_ASSERT(index.model() == this))
            state.push_back(index);
    }

    NX_VERBOSE(this, "Popped state, %1 indices, new stack depth: %2",
        state.size(), d->states.size());

    return state;
}

ResourceTree::FilterType ResourceTreeModelAdapter::filterType() const
{
    return d->filterType;
}

void ResourceTreeModelAdapter::setFilterType(ResourceTree::FilterType value)
{
    if (d->filterType == value)
        return;

    emit filterAboutToBeChanged({});

    d->filterType = value;
    emit filterTypeChanged();

    d->updateFilter();

    emit filterChanged({});
}

QString ResourceTreeModelAdapter::filterText() const
{
    return d->filterText;
}

void ResourceTreeModelAdapter::setFilterText(const QString& value)
{
    const auto trimmedValue = value.trimmed();
    if (d->filterText == trimmedValue)
        return;

    emit filterAboutToBeChanged({});

    d->filterText = trimmedValue;
    emit filterTextChanged();

    d->updateFilter();

    emit filterChanged({});
}

bool ResourceTreeModelAdapter::isFiltering() const
{
    return d->isFiltering;
}

QModelIndex ResourceTreeModelAdapter::rootIndex() const
{
    return d->rootIndex;
}

bool ResourceTreeModelAdapter::localFilesMode() const
{
    return d->localFilesMode;
}

QVector<ResourceTree::ShortcutHint> ResourceTreeModelAdapter::shortcutHints() const
{
    return d->shortcutHints;
}

void ResourceTreeModelAdapter::activateItem(const QModelIndex& index,
    const QModelIndexList& selection,
    const ResourceTree::ActivationType activationType,
    const Qt::KeyboardModifiers modifiers)
{
    if (NX_ASSERT(d->interactionHandler))
        d->interactionHandler->activateItem(index, selection, activationType, modifiers);
}

void ResourceTreeModelAdapter::activateSearchResults(Qt::KeyboardModifiers modifiers)
{
    if (!NX_ASSERT(d->interactionHandler) || d->shortcutHints.isEmpty())
        return;

    d->interactionHandler->activateSearchResults(
        item_model::getLeafIndexes(this, d->rootIndex),
        modifiers);
}

void ResourceTreeModelAdapter::showContextMenu(
    const QPoint& globalPos, const QModelIndex& index, const QModelIndexList& selection)
{
    if (!NX_ASSERT(d->interactionHandler && d->context))
        return;

    d->interactionHandler->showContextMenu(d->context->mainWindowWidget(), globalPos,
        index, selection);
}

bool ResourceTreeModelAdapter::isExtraInfoForced(QnResource* resource) const
{
    return resource && resource->hasFlags(Qn::virtual_camera);
}

bool ResourceTreeModelAdapter::expandsOnDoubleClick(const QModelIndex& index) const
{
    const auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    return !resource || !resource->hasFlags(Qn::layout);
}

bool ResourceTreeModelAdapter::activateOnSingleClick(const QModelIndex& index) const
{
    const auto nodeType = index.data(Qn::NodeTypeRole).value<ResourceTree::NodeType>();
    return nodeType == ResourceTree::NodeType::cloudSystemStatus;
}

bool ResourceTreeModelAdapter::isExtraInfoRequired() const
{
    return qnSettings->resourceInfoLevel() > Qn::RI_NameOnly;
}

ui::action::Parameters ResourceTreeModelAdapter::actionParameters(
    const QModelIndex& index, const QModelIndexList& selection) const
{
    return d->interactionHandler->actionParameters(index, selection);
}

ResourceTree::ExpandedNodeId ResourceTreeModelAdapter::expandedNodeId(
    const QModelIndex& index) const
{
    const auto nodeType = index.data(Qn::NodeTypeRole).value<ResourceTree::NodeType>();
    switch (nodeType)
    {
        case ResourceTree::NodeType::resource:
        {
            const auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
            return NX_ASSERT(resource) &&
                (resource->hasFlags(Qn::server) || resource->hasFlags(Qn::videowall))
                ? ResourceTree::ExpandedNodeId{nodeType, resource->getId().toSimpleString()}
                : ResourceTree::ExpandedNodeId{};
        }

        case ResourceTree::NodeType::recorder:
        case ResourceTree::NodeType::customResourceGroup:
        {
            const auto groupId = nodeType == ResourceTree::NodeType::recorder
                ? index.data(Qn::CameraGroupIdRole).toString()
                : index.data(Qn::ResourceTreeCustomGroupIdRole).toString();

            QString parentId;

            for (auto parent = index.parent(); parent.isValid(); parent = parent.parent())
            {
                const auto type = parent.data(Qn::NodeTypeRole).value<ResourceTree::NodeType>();
                if (type == ResourceTree::NodeType::resource)
                {
                    const auto resource = parent.data(Qn::ResourceRole).value<QnResourcePtr>();
                    if (NX_ASSERT(resource))
                        parentId = resource->getId().toSimpleString();
                    break;
                }

                if (type == ResourceTree::NodeType::camerasAndDevices)
                    break;
            }

            return ResourceTree::ExpandedNodeId{nodeType, groupId, parentId};
        }

        default:
            return isTopLevelNodeType(nodeType)
                ? ResourceTree::ExpandedNodeId{nodeType}
                : ResourceTree::ExpandedNodeId{};
    }
}

void ResourceTreeModelAdapter::setAutoExpandedNodes(
    const std::optional<QSet<ResourceTree::ExpandedNodeId>>& value,
    bool signalModelReset)
{
    const ScopedReset reset(this, signalModelReset);
    d->defaultAutoExpandedNodes = !value.has_value();
    d->autoExpandedNodes = value.value_or(QSet<ResourceTree::ExpandedNodeId>());
}


ResourceTreeModelSquishFacade* ResourceTreeModelAdapter::squishFacade()
{
    return d->squishFacade;
}

void ResourceTreeModelAdapter::registerQmlType()
{
    qmlRegisterType<ResourceTreeModelAdapter>("nx.vms.client.desktop", 1, 0, "ResourceTreeModel");
}

} // nx::vms::client::desktop
