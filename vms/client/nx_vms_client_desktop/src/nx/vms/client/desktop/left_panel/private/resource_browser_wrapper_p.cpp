// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_browser_wrapper_p.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QScopedValueRollback>
#include <QtGui/QAction>

#include <client/client_globals.h>
#include <core/resource/resource.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource_views/resource_tree_settings.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_target_provider.h>
#include <ui/workbench/workbench_context.h>

#include "../left_panel_widget.h"

namespace nx::vms::client::desktop {

namespace {

static const QString kResourceBrowserStateDelegateId = "ResourceBrowser";
static const QString kShowServersInTreeKey = "showServersInTree";
static const QString kExpandedNodesKey = "expandedNodes";

QModelIndex getParent(const QModelIndex& child, int generations)
{
    QModelIndex index = child;
    while (generations > 0 && index.isValid())
    {
        index = index.parent();
        --generations;
    }

    return index;
}

} // namespace

class ResourceBrowserWrapper::StateDelegate: public ClientStateDelegate
{
    ResourceBrowserWrapper* const q;

public:
    StateDelegate(ResourceBrowserWrapper* main): q(main)
    {
    }

    virtual bool loadState(
        const DelegateState& state,
        SubstateFlags flags,
        const StartupParameters& /*params*/) override
    {
        if (!flags.testFlag(ClientStateDelegate::Substate::systemSpecificParameters))
            return false;

        q->context()->resourceTreeSettings()->setShowServersInTree(
            state.value(kShowServersInTreeKey).toBool(/*default value*/ true));

        if (!NX_ASSERT(q->resourceBrowser))
            return false;

        invokeQmlMethod<void>(q->resourceBrowser, "clearFilters");

        const QJsonValue expandedNodes = state.value(kExpandedNodesKey);
        if (expandedNodes.isUndefined()
            || expandedNodes.isNull()
            || !NX_ASSERT(expandedNodes.isArray()))
        {
            q->tree.model()->setAutoExpandedNodes(std::nullopt);
            return true;
        }

        const auto data = expandedNodes.toArray();
        QSet<ResourceTree::ExpandedNodeId> nodeIds;

        for (const auto& item: data)
        {
            if (!NX_ASSERT(item.isObject()))
                continue;

            const auto itemObject = item.toObject();

            bool ok = false;
            const auto nodeType = nx::reflect::fromString<ResourceTree::NodeType>(
                itemObject["type"].toString().toStdString(), &ok);

            const auto id = itemObject["id"].toString();
            if (ok)
                nodeIds.insert({nodeType, id, itemObject["parentId"].toString()});
            else
                NX_WARNING(this, "Unknown stored node type \"%1\"", id);
        }

        q->tree.model()->setAutoExpandedNodes(nodeIds);

        reportStatistics("left_panel_servers_visible",
            q->context()->resourceTreeSettings()->showServersInTree());

        return true;
    }

    virtual void saveState(DelegateState* state, SubstateFlags flags) override
    {
        if (!flags.testFlag(ClientStateDelegate::Substate::systemSpecificParameters))
            return;

        (*state)[kShowServersInTreeKey] = q->context()->resourceTreeSettings()->showServersInTree();

        if (!NX_ASSERT(q->resourceBrowser))
            return;

        if (!q->tree.model()->isFiltering())
            q->acquireClientState();

        QJsonArray expandedNodes;
        for (const auto& nodeId: q->expandedNodeIds)
        {
            QJsonObject data{{"type", QString::fromStdString(nx::reflect::toString(nodeId.type))}};
            if (!nodeId.id.isEmpty())
                data["id"] = nodeId.id;
            if (!nodeId.parentId.isEmpty())
                data["parentId"] = nodeId.parentId;
            expandedNodes.push_back(data);
        }

        (*state)[kExpandedNodesKey] = expandedNodes;
    }
};

ResourceBrowserWrapper::ResourceBrowserWrapper(
    QnWorkbenchContext* context,
    QmlProperty<QQuickItem*> resourceBrowser,
    QWidget* focusScope)
    :
    QnWorkbenchContextAware(context),
    resourceBrowser(resourceBrowser),
    m_layoutInfo(new WorkbenchLayoutInfo(context, this)),
    m_focusScope(focusScope)
{
    appContext()->clientStateHandler()->registerDelegate(
        kResourceBrowserStateDelegateId, std::make_unique<StateDelegate>(this));

    connect(action(ui::action::SearchResourcesAction), &QAction::triggered, this,
        [this]()
        {
            if (m_focusScope)
                m_focusScope->setFocus(Qt::ShortcutFocusReason);
            action(ui::action::ResourcesTabAction)->trigger();
            invokeQmlMethod<void>(this->resourceBrowser, "focusSearchField");
        });

    connect(action(ui::action::EditResourceInTreeAction), &QAction::triggered, this,
        [this]()
        {
            if (m_focusScope)
                m_focusScope->setFocus(Qt::ShortcutFocusReason);
            action(ui::action::ResourcesTabAction)->trigger();
            tree.startEditing();
        });

    connect(this, &ResourceBrowserWrapper::resourceSelectionChanged, this,
        [this]
        {
            if (m_inSelection)
                return;

            QScopedValueRollback<bool> guard(m_inSelection, true);
            action(ui::action::SelectionChangeAction)->trigger();
        });

    connect(action(ui::action::SelectionChangeAction), &QAction::triggered,
        this, &ResourceBrowserWrapper::clearResourceSelection);

    connect(action(ui::action::SelectNewItemAction), &QAction::triggered,
        this, &ResourceBrowserWrapper::handleNewResourceItemAction);

    connect(action(ui::action::CreateNewCustomGroupAction), &QAction::triggered,
        this, &ResourceBrowserWrapper::beforeGroupProcessing);

    connect(action(ui::action::RenameCustomGroupAction), &QAction::triggered,
        this, &ResourceBrowserWrapper::beforeGroupProcessing);

    connect(action(ui::action::NewCustomGroupCreatedEvent), &QAction::triggered, this,
        [this]() { afterGroupProcessing(ui::action::NewCustomGroupCreatedEvent); });

    connect(action(ui::action::CustomGroupRenamedEvent), &QAction::triggered, this,
        [this]() { afterGroupProcessing(ui::action::CustomGroupRenamedEvent); });

    // Initialization after the QML component is loaded.
    tree.connectNotifySignal(
        [this]()
        {
            tree.scene = QVariant::fromValue(m_layoutInfo.get());
            connect(tree, SIGNAL(selectionChanged()), this, SIGNAL(resourceSelectionChanged()));

            connect(tree.model(), &ResourceTreeModelAdapter::filterAboutToBeChanged, this,
                [this]()
                {
                    if (!tree.model()->isFiltering())
                        acquireClientState();
                });
        });
}

ResourceBrowserWrapper::~ResourceBrowserWrapper()
{
    appContext()->clientStateHandler()->unregisterDelegate(kResourceBrowserStateDelegateId);
}

ui::action::Parameters ResourceBrowserWrapper::currentParameters() const
{
    if (!tree)
        return ui::action::Parameters();

    return tree.model.value()->actionParameters(
        tree.currentIndex,
        tree.selection());
}

void ResourceBrowserWrapper::clearResourceSelection()
{
    if (!m_inSelection && tree)
        tree.clearSelection();
}

void ResourceBrowserWrapper::handleNewResourceItemAction()
{
    if (!NX_ASSERT(!m_inSelection) || !tree)
        return;

    QScopedValueRollback<bool> guard(m_inSelection, true);

    const auto parameters = menu()->currentParameters(sender());
    const auto model = tree.model.value();

    int role = -1;
    QVariant value;

    if (parameters.hasArgument(Qn::UuidRole))
    {
        role = Qn::UuidRole;
        value = parameters.argument(Qn::UuidRole);
    }
    else if (const auto resource = parameters.resource())
    {
        role = Qn::ResourceRole;
        value = QVariant::fromValue(resource);
    }
    else
    {
        return;
    }

    const auto indexes = model->match(
        model->index(0, 0),
        role,
        value,
        1,
        Qt::MatchFlags(Qt::MatchExactly | Qt::MatchRecursive));

    action(ui::action::ResourcesTabAction)->trigger();
    tree.setSelection(indexes);
}

std::pair<QModelIndex, int /*depth*/> ResourceBrowserWrapper::findResourceIndex(
    const QModelIndex& parent) const
{
    int level = 0;
    QModelIndex index = parent;

    while (index.isValid() && !index.data(Qn::ResourceRole).value<QnResourcePtr>())
    {
        index = index.model()->index(0, 0, index);
        ++level;
    }

    const int depth = index.isValid() ? level : -1;
    return {index, depth};
}

void ResourceBrowserWrapper::beforeGroupProcessing()
{
    if (!tree)
        return;

    m_nestedResourceIndex = QModelIndex();
    m_nestedResourceLevel = -1;

    const auto selection = tree.selection();
    if (selection.empty())
        return;

    auto [index, level] = findResourceIndex(selection[0]);
    if (!index.isValid())
        return;

    // If explicitly selected a recorder's child.
    if (level == 0 && index.parent().data(Qn::NodeTypeRole).value<ResourceTree::NodeType>()
        == ResourceTree::NodeType::recorder)
    {
        ++level;
    }

    m_nestedResourceIndex = index;
    m_nestedResourceLevel = level;

    saveGroupExpandedState(selection);
}

void ResourceBrowserWrapper::afterGroupProcessing(ui::action::IDType eventType)
{
    const auto cleanupGuard = nx::utils::makeScopeGuard(
        [this]()
        {
            m_groupExpandedState = {};
            m_nestedResourceIndex = QModelIndex();
        });

    if (!tree || !NX_ASSERT(m_nestedResourceIndex.isValid()))
        return;

    if (eventType == ui::action::NewCustomGroupCreatedEvent)
        ++m_nestedResourceLevel;
    else if (!NX_ASSERT(eventType == ui::action::CustomGroupRenamedEvent))
        return;

    QModelIndex index = m_nestedResourceIndex;
    for (int i = 0; i < m_nestedResourceLevel; ++i)
        index = index.parent();

    if (NX_ASSERT(index.isValid()))
    {
        tree.setCurrentIndex(index);
        tree.setExpanded(index, true);

        restoreGroupExpandedState(index);

        if (eventType == ui::action::NewCustomGroupCreatedEvent)
            tree.startEditing();
    }
}

void ResourceBrowserWrapper::saveGroupExpandedState(const QModelIndexList& within)
{
    m_groupExpandedState.clear();
    QModelIndexList expandedGroupIndexes;

    const auto processIndexRecursively = nx::utils::y_combinator(
        // Returns true if has expanded sub-groups or recorders, false otherwise.
        [this, &expandedGroupIndexes](
            auto processIndexRecursively, const QModelIndex& index) -> bool
        {
            const auto nodeType = index.data(Qn::NodeTypeRole).value<ResourceTree::NodeType>();
            if ((nodeType != ResourceTree::NodeType::customResourceGroup
                && nodeType != ResourceTree::NodeType::recorder)
                || !invokeQmlMethod<bool>(tree, "isExpanded", index))
            {
                return false;
            }

            const int rowCount = index.model()->rowCount(index);
            bool hasExpandedChildren = false;

            for (int i = 0; i < rowCount; ++i)
                hasExpandedChildren |= processIndexRecursively(index.model()->index(i, 0, index));

            if (!hasExpandedChildren)
                expandedGroupIndexes.push_back(index);

            return true;
        });

    for (const auto index: within)
    {
        if (index.isValid())
           processIndexRecursively(index);
    }

    for (const auto groupIndex: expandedGroupIndexes)
    {
        const auto [resourceIndex, depth] = findResourceIndex(groupIndex);
        const auto resource = resourceIndex.data(Qn::ResourceRole).value<QnResourcePtr>();
        if (NX_ASSERT(resource && depth > 0))
            m_groupExpandedState[resource] = depth - 1;
    }
}

void ResourceBrowserWrapper::restoreGroupExpandedState(const QModelIndex& under)
{
    if (!NX_ASSERT(under.isValid()))
        return;

    QModelIndexList indexesToEnsureVisibility;

    const auto processIndexRecursively = nx::utils::y_combinator(
        [this, &indexesToEnsureVisibility](
            auto processIndexRecursively, const QModelIndex& index) -> void
        {
            const auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
            const int shift = m_groupExpandedState.value(resource, -1);
            if (shift >= 0)
            {
                const auto indexToEnsureVisibility = getParent(index, shift);
                if (indexToEnsureVisibility.isValid())
                    indexesToEnsureVisibility.push_back(indexToEnsureVisibility);
            }

            const int rowCount = index.model()->rowCount(index);
            for (int i = 0;
                i < rowCount && indexesToEnsureVisibility.size() < m_groupExpandedState.size();
                ++i)
            {
                processIndexRecursively(index.model()->index(i, 0, index));
            }
        });

    processIndexRecursively(under);
    invokeQmlMethod<void>(tree, "ensureVisible", indexesToEnsureVisibility);
}

void ResourceBrowserWrapper::acquireClientState()
{
    if (!NX_ASSERT(!tree.model()->isFiltering()))
        return;

    const auto expandedIndexes = nx::utils::toTypedQList<QModelIndex>(
        invokeQmlMethod<QVariantList>(tree(), "expandedState"));

    expandedNodeIds.clear();
    for (const auto& index: expandedIndexes)
    {
        if (const auto expandedNodeId = tree.model()->expandedNodeId(index))
            expandedNodeIds.insert(expandedNodeId);
    }
}

} // namespace nx::vms::client::desktop
