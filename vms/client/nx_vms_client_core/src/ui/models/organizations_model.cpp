// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "organizations_model.h"

#include <ranges>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaObject>

#include <nx/utils/coro/task_utils.h>
#include <nx/utils/coro/when_all.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/scoped_connections.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/client/core/common/models/linearization_list_model.h>
#include <nx/vms/client/core/network/cloud_api.h>

namespace nx::vms::client::core {

namespace {

using namespace std::chrono;

static constexpr auto kErrorRetryDelay = 5s;
static constexpr auto kUpdateDelay = 20s;
static constexpr size_t kMaxConcurrentRequests = 10;
static constexpr int kMaximumPageSize = 1000;

struct TreeNode
{
    using Map = std::unordered_map<nx::Uuid, TreeNode*>;

    TreeNode(
        Map& idToNode,
        nx::Uuid id = {},
        QString name = {},
        OrganizationsModel::NodeType type = OrganizationsModel::None)
        :
        id(id),
        name(name),
        type(type),
        idToNode(idToNode)
    {
        idToNode.insert({id, this});
    }

    TreeNode(Map& idToNode, ChannelPartner cp)
        : TreeNode(
            idToNode,
            cp.id,
            QString::fromStdString(cp.name),
            OrganizationsModel::ChannelPartner)
    {
        state = cp.effectiveState;
        loading = true;
    }

    TreeNode(Map& idToNode, Organization org)
        : TreeNode(
            idToNode,
            org.id,
            QString::fromStdString(org.name),
            OrganizationsModel::Organization)
    {
        systemCount = org.systemCount;
        state = org.effectiveState;
        orgOwnRoleIds = {org.ownRolesIds.begin(), org.ownRolesIds.end()};
        loading = true;
    }

    TreeNode(Map& idToNode, GroupStructure group)
        : TreeNode(
            idToNode,
            group.id,
            QString::fromStdString(group.name),
            OrganizationsModel::Folder)
    {
        systemCount = group.systemCount;
    }

    TreeNode(Map& idToNode, SystemInOrganization system)
        : TreeNode(
            idToNode,
            system.systemId,
            QString::fromStdString(system.name),
            OrganizationsModel::System)
    {
        state = system.effectiveState;
    }

    class Registry
    {
    public:
        template<typename... Args>
        std::unique_ptr<TreeNode> create(Args&&... args)
        {
            auto node = std::make_unique<TreeNode>(idToNode, std::forward<Args>(args)...);
            return node;
        }

        TreeNode* find(nx::Uuid id) const
        {
            if (auto it = idToNode.find(id); it != idToNode.end())
                return it->second;

            return nullptr;
        }

    private:
        TreeNode::Map idToNode;
    };

    ~TreeNode()
    {
        idToNode.erase(id);
    }

    int row() const
    {
        if (!parent)
            return 0;

        auto it = std::find_if(
            parent->children.cbegin(),
            parent->children.cend(),
            [this](const std::unique_ptr<TreeNode>& node)
            {
                return node.get() == this;
            });

        return std::distance(parent->children.cbegin(), it);
    }

    void append(std::unique_ptr<TreeNode>&& node)
    {
        insert(children.size(), std::move(node));
    }

    void insert(int row, std::unique_ptr<TreeNode>&& node)
    {
        node->parent = this;
        children.insert(children.begin() + row, std::move(node));
    }

    void remove(int row)
    {
        children.erase(children.begin() + row);
    }

    void clear()
    {
        children.clear();
    }

    TreeNode* parentNode() const
    {
        return parent;
    }

    const std::vector<std::unique_ptr<TreeNode>>& allChildren() const
    {
        return children;
    }

    bool hasOrganizationRole(const nx::Uuid& id) const
    {
        if (type == OrganizationsModel::Organization)
            return orgOwnRoleIds.contains(id);

        return parentNode() && parentNode()->hasOrganizationRole(id);
    }

    nx::Uuid id;
    QString name;
    int systemCount = -1;
    api::SaasState state = api::SaasState::uninitialized;
    std::unordered_set<nx::Uuid> orgOwnRoleIds;
    OrganizationsModel::NodeType type = OrganizationsModel::None;
    bool loading = false;
    bool accessible = true;

private:
    TreeNode* parent = nullptr;
    std::vector<std::unique_ptr<TreeNode>> children;
    Map& idToNode;
};

inline bool containsId(const std::vector<nx::Uuid>& ids, const nx::Uuid& id)
{
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

bool canAccess(const Organization& org)
{
    // Organization is accessible if the user has any role in it.
    return !org.ownRolesIds.empty();
}

bool canAccess(const ChannelPartner& cp)
{
    return !containsId(cp.ownRolesIds, kChannelPartnerReportsViewerId);
}

// Checks if the organization can be accessed within the parent channel partner.
bool canAccess(const Organization& org, const ChannelPartner& parentCp)
{
    const QSet<nx::Uuid> cpRolesIds(parentCp.ownRolesIds.begin(), parentCp.ownRolesIds.end());
    if (cpRolesIds.contains(kChannelPartnerReportsViewerId))
        return false;

    const auto accessLevel = nx::Uuid::fromStringSafe(org.channelPartnerAccessLevel);

    if (accessLevel.isNull()
        && (cpRolesIds.contains(kChannelPartnerAdministratorId)
            || cpRolesIds.contains(kChannelPartnerManagerId)))
    {
        return false;
    }

    return true;
}

} // namespace

struct OrganizationsModel::Private
{
    OrganizationsModel* const q;

    QPointer<CloudStatusWatcher> statusWatcher;
    QPointer<QAbstractItemModel> sitesModel; //< Goes under special 'Sites' node.
    int sitesModelRowCount = 0;
    nx::utils::ScopedConnections connections;

    bool hasChannelPartners = false;
    bool hasOrganizations = false;

    TreeNode::Registry nodes; //< Should be destroyed after root.
    std::unique_ptr<TreeNode> root;

    QSet<nx::Uuid> accessibleOrgs;
    bool topLevelLoading = false;
    uint64_t updateGeneration = 0;

    nx::utils::ScopedConnection statusConnection;
    CloudStatusWatcher::Status prevStatus = CloudStatusWatcher::LoggedOut;

    QPersistentModelIndex currentRoot;

    // Cache for change detection to avoid unnecessary model resets
    std::unordered_map<nx::Uuid /*orgId*/, std::vector<GroupStructure>> orgStructureCache;
    // Cache systems per organization
    std::unordered_map<
        nx::Uuid /*orgId*/,
        std::unordered_map<nx::Uuid /*systemId*/, SystemInOrganization>> orgSystemsCache;

    Private(OrganizationsModel* q): q(q)
    {
        // Create root node and 'Sites' node as it's child at index 0. 'Sites' node will always be
        // at index 0.
        root = nodes.create();
        auto sites = nodes.create(nx::Uuid::createUuid(), "Sites");
        sites->type = OrganizationsModel::SitesNode;
        root->append(std::move(sites));
    }

    // Insert single node at index 'row' under 'parent'. Report model changes.
    void insert(int row, std::unique_ptr<TreeNode> node, TreeNode* parent)
    {
        if (parent == nullptr)
            parent = root.get();

        auto parentIndex = parent == root.get()
            ? QModelIndex()
            : q->createIndex(parent->row(), 0, parent);

        const ScopedInsertRows insertRows(q, parentIndex, row, row);
        parent->insert(row, std::move(node));
    }

    // Remove single node at index 'row' under 'parent'. Report model changes.
    void remove(int row, TreeNode* parent)
    {
        if (!parent)
            parent = root.get();

        auto parentIndex = parent == root.get()
            ? QModelIndex()
            : q->createIndex(parent->row(), 0, parent);

        const ScopedRemoveRows removeRows(q, parentIndex, row, row);
        parent->remove(row);
    }

    // Remove all nodes under 'parent'. Report model changes.
    void clear(TreeNode* parent)
    {
        if (!parent)
            parent = root.get();

        auto parentIndex = parent == root.get()
            ? QModelIndex()
            : q->createIndex(parent->row(), 0, parent);

        if (parent->allChildren().size() == 0)
            return;

        const ScopedRemoveRows removeRows(q, parentIndex, 0, parent->allChildren().size() - 1);
        parent->clear();
    }

    void setChannelPartners(const ChannelPartnerList& data)
    {
        auto runAtScopeExit = nx::utils::ScopeGuard(
            [this, empty = data.results.empty()]()
            {
                setHasChannelPartners(!empty);
            });

        std::unordered_set<nx::Uuid> prevIds;
        for (const auto& node: root->allChildren())
        {
            if (node->type == OrganizationsModel::ChannelPartner)
                prevIds.insert(node->id);
        }

        // Add or update channel partners.
        for (const auto& partner: data.results)
        {
            if (!prevIds.contains(partner.id))
            {
                insert(
                    root->allChildren().size(),
                    nodes.create(partner),
                    root.get());
                continue;
            }
            prevIds.erase(partner.id);
            auto node = nodes.find(partner.id);
            if (node->name != QString::fromStdString(partner.name)
                || node->state != partner.effectiveState)
            {
                node->name = QString::fromStdString(partner.name);
                node->state = partner.effectiveState;
                notifyNodeUpdate(node);
            }
        }

        // Remove channel partners that are not present here.
        for (const auto& id: prevIds)
        {
            if (auto node = nodes.find(id))
                remove(node->row(), node->parentNode());
        }
    }

    void setOrganizations(
        const std::vector<nx::vms::client::core::Organization> orgs,
        nx::Uuid parentId)
    {
        auto runAtScopeExit = nx::utils::ScopeGuard(
            [this, parentId, empty = orgs.empty()]()
            {
                if (parentId == root->id)
                    setHasOrganizations(!empty);
            });

        // Either channel partner or root node.
        auto parent = nodes.find(parentId);
        if (!parent)
            return;

        std::unordered_set<nx::Uuid> prevIds;
        for (const auto& node: parent->allChildren())
        {
            if (node->type == OrganizationsModel::Organization)
                prevIds.insert(node->id);
        }

        // Add or update organizations.
        for (const auto& org: orgs)
        {
            if (!prevIds.contains(org.id))
            {
                insert(
                    parent->allChildren().size(),
                    nodes.create(org),
                    parent);
                continue;
            }
            prevIds.erase(org.id);
            auto node = nodes.find(org.id);
            if (!node)
                continue;

            const std::unordered_set<nx::Uuid> ownRoleIds =
                {org.ownRolesIds.begin(), org.ownRolesIds.end()};

            if (node->name != QString::fromStdString(org.name)
                || node->systemCount != org.systemCount
                || node->state != org.effectiveState
                || node->orgOwnRoleIds != ownRoleIds)
            {
                node->name = QString::fromStdString(org.name);
                node->systemCount = org.systemCount;
                node->state = org.effectiveState;
                node->orgOwnRoleIds = ownRoleIds;
                notifyNodeUpdate(node);
            }
        }

        // Remove organizations that are not present here.
        for (const auto& id: prevIds)
        {
            if (auto node = nodes.find(id))
                remove(node->row(), node->parentNode());
        }
    }

    // Set group (folder) structure under 'parent' node recursively.
    void setNodeGroups(TreeNode* parent, const std::vector<GroupStructure>& groups)
    {
        std::unordered_set<nx::Uuid> prevIds;

        for (const auto& child: parent->allChildren())
        {
            if (child->type == OrganizationsModel::Folder)
                prevIds.insert(child->id);
        }

        for (const auto& group: groups)
        {
            if (prevIds.contains(group.id))
            {
                // We already have this group under current parent. Update it.
                if (auto node = nodes.find(group.id))
                {
                    if (node->name != group.name || node->systemCount != group.systemCount)
                    {
                        node->name = QString::fromStdString(group.name);
                        node->systemCount = group.systemCount;
                        notifyNodeUpdate(node);
                    }
                    setNodeGroups(node, group.children);
                }
                prevIds.erase(group.id);
                continue;
            }

            if (auto node = nodes.find(group.id))
            {
                // We already have this group but under a different parent. Remove it.
                remove(node->row(), node->parentNode());
                prevIds.erase(group.id);
            }

            // Append new group.
            auto node = nodes.create(group);
            setNodeGroups(node.get(), group.children);
            insert(parent->allChildren().size(), std::move(node), parent);
        }

        // Remove groups that are not present here.
        for (const auto& id: prevIds)
        {
            if (auto node = nodes.find(id))
                remove(node->row(), node->parentNode());
        }
    }

    // Set group (folder) structure under organization node.
    void setOrgStructure(nx::Uuid orgId, const std::vector<GroupStructure>& data)
    {
        auto orgNode = nodes.find(orgId);

        if (!orgNode)
            return;

        const auto cachedIt = orgStructureCache.find(orgId);
        if (cachedIt != orgStructureCache.end() && cachedIt->second == data)
            return;

        if (data.empty())
        {
            clear(orgNode);
            if (cachedIt != orgStructureCache.end())
                orgStructureCache.erase(cachedIt);
            return;
        }

        setNodeGroups(orgNode, data);

        if (cachedIt != orgStructureCache.end())
            cachedIt->second = data;
        else
            orgStructureCache.emplace(orgId, data);
    }

    void gatherSystems(TreeNode* parent, std::unordered_set<nx::Uuid>& systems)
    {
        for (const auto& child: parent->allChildren())
        {
            if (child->type == OrganizationsModel::System)
                systems.insert(child->id);
            else
                gatherSystems(child.get(), systems);
        }
    }

    void setOrgSystems(nx::Uuid orgId, const std::vector<SystemInOrganization>& data)
    {
        // Updating group structure (e.g. folder move) may have deleted some systems.

        auto orgNode = nodes.find(orgId);
        if (!orgNode)
            return;

        auto& cachedSystems = orgSystemsCache[orgId];
        std::unordered_map<nx::Uuid, SystemInOrganization> newSystems;
        for (const auto& system: data)
            newSystems[system.systemId] = system;

        const auto needsUpdate = [&cachedSystems](const auto& it)
            {
                const auto cachedIt = cachedSystems.find(it.first);
                return cachedIt == cachedSystems.end() || cachedIt->second != it.second;
            };

        const bool hasChanges = (cachedSystems.size() != newSystems.size())
            || std::any_of(newSystems.cbegin(), newSystems.cend(), needsUpdate);

        if (!hasChanges)
            return;

        for (const auto& [systemId, _]: cachedSystems)
        {
            if (!newSystems.contains(systemId))
            {
                if (const auto node = nodes.find(systemId))
                    remove(node->row(), node->parentNode());
            }
        }

        // Add new or update existing systems
        for (const auto& system: data)
        {
            if (!needsUpdate(std::pair{system.systemId, system}))
                continue;

            auto parentNode = nodes.find(system.groupId.isNull() ? orgId : system.groupId);
            if (!parentNode)
                continue;

            int row = parentNode->allChildren().size();

            if (auto systemNode = nodes.find(system.systemId)) //< Replace existing system.
            {
                const int oldRow = systemNode->row();
                row = systemNode->parentNode() == parentNode ? oldRow : std::max(row - 1, 0);
                remove(oldRow, systemNode->parentNode());
            }

            insert(row, nodes.create(system), parentNode);
        }

        cachedSystems = std::move(newSystems);
    }

    QModelIndex sitesRoot() const
    {
        return q->index(0, 0);
    }

    QModelIndex mapFromProxyModel(const QModelIndex& index) const
    {
        if (!sitesModel)
            return {};

        return q->index(index.row(), index.column(), sitesRoot());
    }

    QModelIndex mapToProxyModel(const QModelIndex& index) const
    {
        return sitesModel->index(index.row(), index.column());
    }

    void setTopLevelLoading(bool loading)
    {
        if (topLevelLoading == loading)
            return;

        topLevelLoading = loading;
        emit q->topLevelLoadingChanged();
    }

    void beginSitesReset()
    {
        const int prevRowCount = sitesModel->rowCount();
        if (prevRowCount == 0)
            return;

        const ScopedRemoveRows removeRows(q, sitesRoot(), 0, prevRowCount - 1);
        sitesModelRowCount = 0;
    }

    void endSitesReset()
    {
        const int newRowCount = sitesModel->rowCount();
        if (newRowCount == 0)
            return;

        const ScopedInsertRows insertRows(q, sitesRoot(), 0, newRowCount - 1);
        sitesModelRowCount = newRowCount;
    }

    void setHasChannelPartners(bool value);
    void setHasOrganizations(bool value);

    coro::FireAndForget startPolling();
    coro::Task<bool> loadOrgAsync(struct Organization org);
    coro::Task<bool> loadOrgListAsync(OrganizationList orgList);
    coro::Task<bool> loadChannelPartnerOrgsAsync(ChannelPartnerList cpList);

    static void fillOrgInfo(
        std::vector<nx::vms::client::core::Organization>& organizations,
        const QHash<nx::Uuid, nx::vms::client::core::Organization>& organizationsCache)
    {
        for (auto& org: organizations)
        {
            if (auto it = organizationsCache.find(org.id); it != organizationsCache.end())
                org = it.value();
        }
    }

    void removeInactiveSystems(std::vector<nx::vms::client::core::SystemInOrganization>& systems)
    {
        const auto isActivatedOrPending =
            [this](const auto& system) -> bool
            {
                if (system.system_state == cloud::db::api::SystemStatus::activated)
                    return true;

                const auto id = system.systemId.toSimpleString();

                const bool isPending =
                    system.system_state == cloud::db::api::SystemStatus::notActivated
                    && q->indexFromSystemId(id).data(QnSystemsModel::IsPending).toBool();

                return isPending;
            };

        systems.erase(
            std::remove_if(
                systems.begin(),
                systems.end(),
                [&](const auto& system) { return !isActivatedOrPending(system); }),
            systems.end());
    }

    void clearCloudData();

    void notifyNodeUpdate(
        const TreeNode* node,
        const QList<int>& roles = {},
        bool recursively = false)
    {
        if (!node)
            return;

        const auto index = q->createIndex(node->row(), 0, node);
        q->dataChanged(index, index, roles);

        if (recursively)
        {
            for (const auto& child: node->allChildren())
                notifyNodeUpdate(child.get(), roles, recursively);
        }
    }

    void updateNodeAccess(TreeNode* node, bool accessible)
    {
        if (node->accessible == accessible)
            return;

        QList<int> updateRoles;

        node->accessible = accessible;
        updateRoles << OrganizationsModel::IsAccessDeniedRole;

        if (!accessible && node->loading)
        {
            node->loading = false;
            updateRoles << OrganizationsModel::IsLoadingRole;
        }

        notifyNodeUpdate(node, updateRoles);
    }

    template <typename T, typename... P>
        requires ((std::is_same_v<T, struct ChannelPartner>
                || std::is_same_v<T, struct Organization>)
            && (sizeof...(P) == 0
                || std::is_same_v<std::tuple<P...>, std::tuple<struct ChannelPartner>>))
    void removeInaccessibleItems(std::vector<T>* items, P... params)
    {
        items->erase(
            std::remove_if(
                items->begin(),
                items->end(),
                [&](const auto& item)
                {
                    const bool accessible = canAccess(item, params...);
                    if (auto node = this->nodes.find(item.id))
                        updateNodeAccess(node, accessible);
                    return !accessible;
                }),
            items->end());
    }

    template <typename T, typename... P>
        requires ((std::is_same_v<T, struct ChannelPartner>
                || std::is_same_v<T, struct Organization>)
            && (sizeof...(P) == 0
                || std::is_same_v<std::tuple<P...>, std::tuple<struct ChannelPartner>>))
    void updateItemsAccess(const std::vector<T>& items, P... params)
    {
        for (const auto& item: items)
        {
            const bool accessible = canAccess(item, params...);
            if (auto node = this->nodes.find(item.id))
                updateNodeAccess(node, accessible);
        }
    }
};

OrganizationsModel::OrganizationsModel(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

OrganizationsModel::~OrganizationsModel()
{}

QModelIndex OrganizationsModel::indexFromNodeId(nx::Uuid id) const
{
    if (auto node = d->nodes.find(id))
        return createIndex(node->row(), 0, node);

    return {};
}

QModelIndex OrganizationsModel::indexFromSystemId(const QString& id) const
{
    if (d->sitesModel)
    {
        auto systemsModel = qobject_cast<QnSystemsModel*>(d->sitesModel.get());
        if (!systemsModel)
            return {};

        auto row = systemsModel->getRowIndex(id);
        if (row == -1)
            return {};

        return d->mapFromProxyModel(systemsModel->index(row, 0));
    }

    return {};
}

int OrganizationsModel::rowCount(const QModelIndex& parent) const
{
    if (parent.column() > 0)
        return 0;

    if (!d->root)
        return 0;

    const TreeNode* parentNode = parent.isValid()
        ? static_cast<const TreeNode*>(parent.internalPointer())
        : d->root.get();

    if (!parentNode)
        return 0;

    // Do not use d->sitesModel->rowCount() directly as it requires special handling during model
    // reset.
    if (parentNode->type == OrganizationsModel::SitesNode)
        return d->sitesModelRowCount;

    return parentNode->allChildren().size();
}

int OrganizationsModel::columnCount(const QModelIndex&) const
{
    return 1;
}

QVariant OrganizationsModel::data(const QModelIndex& index, int role) const
{
    static const QString kChannelPartners = tr("Partners");
    static const QString kOrganizations = tr("Organizations");
    static const QString kFolders = tr("Folders");
    static const QString kSites = tr("Sites");

    if (!index.isValid())
        return {};

    const auto* node = static_cast<const TreeNode*>(index.internalPointer());

    if (!node)
    {
        if (!d->sitesModel)
            return {};

        if (index.parent() != d->sitesRoot())
            return {};

        switch (role)
        {
            case Qt::DisplayRole:
                role = QnSystemsModel::SystemNameRoleId;
                break;
            case TypeRole:
                return QVariant::fromValue(OrganizationsModel::System);
            case IsFromSitesRole:
                return true;
            case TabSectionRole:
                return OrganizationsModel::SitesTab;
            case SectionRole:
            case PathFromRootRole:
                return d->sitesModel->data(
                    d->mapToProxyModel(index), QnSystemsModel::IsCloudSystemRoleId).toBool()
                        ? tr("Cloud")
                        : tr("Local");
            case QnSystemsModel::IsSaasUninitialized:
            {
                auto siteIndex = d->mapToProxyModel(index);

                // System has organization id? Definitely SaaS is initialized.
                if (!d->sitesModel->data(
                    siteIndex, QnSystemsModel::OrganizationIdRoleId).value<QUuid>().isNull())
                {
                    return false;
                }
                return d->sitesModel->data(siteIndex, role);
            }
            case OrganizationsModel::IsAccessibleThroughOrgRole:
            {
                const auto parentOrgId = d->sitesModel->data(
                    d->mapToProxyModel(index),
                    QnSystemsModel::OrganizationIdRoleId).value<QUuid>();

                return d->accessibleOrgs.contains(parentOrgId);
            }
        }
        return d->sitesModel->data(d->mapToProxyModel(index), role);
    }

    switch (role)
    {
        case Qt::DisplayRole:
        case QnSystemsModel::SystemNameRoleId:
            return node->name;
        case IdRole:
            return QVariant::fromValue(node->id);
        case QnSystemsModel::SystemIdRoleId:
            return node->id.toSimpleString();
        case SytemCountRole:
            return node->systemCount;
        case TypeRole:
            return QVariant::fromValue(node->type);
        case SectionRole:
            switch (node->type)
            {
                case OrganizationsModel::ChannelPartner:
                    return kChannelPartners;
                case OrganizationsModel::Organization:
                    return kOrganizations;
                case OrganizationsModel::Folder:
                    return kFolders;
                case OrganizationsModel::System:
                    return kSites;
                default:
                    return "";
            }
            break;
        case PathFromRootRole:
        {
            std::vector<QModelIndex> parents;

            for (auto parent = index.parent(); parent.isValid(); parent = parent.parent())
                parents.push_back(parent);

            if (parents.empty())
                return data(index, OrganizationsModel::SectionRole);

            QStringList sectionNameParts;
            for (const auto& parent: nx::utils::reverseRange(parents))
                sectionNameParts << parent.data(Qt::DisplayRole).toString();

            return sectionNameParts.join(" / ");
        }
        case IsLoadingRole:
            return node->loading;
        case IsFromSitesRole:
            return false;
        case TabSectionRole:
        {
            for (auto current = node; current; current = current->parentNode())
            {
                if (current->parentNode() == d->root.get())
                {
                    return current->type == OrganizationsModel::Organization
                        ? OrganizationsModel::OrganizationsTab
                        : OrganizationsModel::ChannelPartnersTab;
                }
            }
            NX_ASSERT(false, "Node is not under root");
            return OrganizationsModel::SitesTab;
        }
        case IsAdministratorRole:
            return node->hasOrganizationRole(kOrganizationAdministratorId);
        case QnSystemsModel::IsSaasUninitialized:
            return node->type == SitesNode;
        case QnSystemsModel::IsSaasSuspended:
            return node->state == api::SaasState::suspended;
        case QnSystemsModel::IsSaasShutDown:
            return node->state == api::SaasState::shutdown;
        case IsAccessibleThroughOrgRole:
            return true;
        case IsAccessDeniedRole:
            return !node->accessible;
        default:
            if (node->type == OrganizationsModel::System && d->sitesModel)
            {
                auto systemsModel = qobject_cast<QnSystemsModel*>(d->sitesModel.get());
                if (!systemsModel)
                    return {};

                const int rowInSystems = systemsModel->getRowIndex(node->id.toSimpleString());
                if (rowInSystems == -1)
                    return {};

                return systemsModel->index(rowInSystems, 0).data(role);
            }
            break;
    }
    return {};
}

QModelIndex OrganizationsModel::parent(const QModelIndex& index) const
{
    if (!index.isValid())
        return {};

    if (!d->root)
        return {};

    if (!index.internalPointer())
        return sitesRoot();

    auto* childNode = static_cast<TreeNode*>(index.internalPointer());

    TreeNode* parentNode = childNode->parentNode();

    if (!parentNode)
        return {};

    return parentNode != d->root.get()
        ? createIndex(parentNode->row(), 0, parentNode)
        : QModelIndex{};
}

QModelIndex OrganizationsModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return {};

    if (!d->root)
        return {};

    TreeNode* parentNode = parent.isValid()
        ? static_cast<TreeNode*>(parent.internalPointer())
        : d->root.get();

    if (parentNode->type == OrganizationsModel::SitesNode)
        return createIndex(row, column);

    if (auto* childNode = parentNode->allChildren().at(row).get())
        return createIndex(row, column, childNode);

    return {};
}

bool OrganizationsModel::hasChildren(const QModelIndex& parent) const
{
    if (!d->root)
        return false;

    const TreeNode* parentNode = parent.isValid()
        ? static_cast<TreeNode*>(parent.internalPointer())
        : d->root.get();

    return parentNode && parentNode->type != OrganizationsModel::System;
}

QStringList OrganizationsModel::childSystemIds(const QModelIndex& parent) const
{
    if (!d->root)
        return {};

    QStringList result;
    auto addSystemIds = nx::utils::y_combinator(
        [&](auto addSystemIds, const TreeNode* node) -> void
        {
            if (node->type == NodeType::System)
                result.append(node->id.toSimpleString());

            for (const auto& child: node->allChildren())
                addSystemIds(child.get());
        });

    addSystemIds(parent.isValid()
        ? static_cast<TreeNode*>(parent.internalPointer())
        : d->root.get());

    return result;
}

QHash<int, QByteArray> OrganizationsModel::roleNames() const
{
    static const QHash<int, QByteArray> kRoleNames{
        {Qt::DisplayRole, "display"},
        {IdRole, "nodeId"},
        {TypeRole, "type"},
        {SytemCountRole, "systemCount"},
        {SectionRole, "section"},
        {IsLoadingRole, "isLoading"},
        {IsFromSitesRole, "isFromSites"},
        {IsAccessibleThroughOrgRole, "isAccessibleThroughOrg"},
        {IsAccessDeniedRole, "isAccessDenied"},
        {IsAdministratorRole, "isAdministrator"},
        {PathFromRootRole, "pathFromRoot"},
    };

    auto roles = QnSystemsModel::kRoleNames;
    roles.insert(kRoleNames);
    return roles;
}

CloudStatusWatcher* OrganizationsModel::statusWatcher() const
{
    return d->statusWatcher;
}

void OrganizationsModel::setStatusWatcher(CloudStatusWatcher* statusWatcher)
{
    if (d->statusWatcher == statusWatcher)
        return;

    d->statusConnection.reset();
    d->statusWatcher = statusWatcher;
    emit statusWatcherChanged();

    if (!d->statusWatcher)
        return;

    // Set topLevelLoading = true when status changes from LoggedOut to any other status.
    d->statusConnection.reset(connect(d->statusWatcher, &CloudStatusWatcher::statusChanged, this,
        [this]()
        {
            auto newStatus = d->statusWatcher->status();
            if (d->prevStatus == CloudStatusWatcher::LoggedOut
                && newStatus != CloudStatusWatcher::LoggedOut)
            {
                d->startPolling();
            }
            else if (newStatus == CloudStatusWatcher::LoggedOut)
            {
                d->setTopLevelLoading(false);
                d->clearCloudData();
            }
            d->prevStatus = newStatus;
        },
        Qt::QueuedConnection));

    d->prevStatus = d->statusWatcher->status();

    if (d->prevStatus != CloudStatusWatcher::LoggedOut)
        d->startPolling();
}

QAbstractItemModel* OrganizationsModel::systemsModel() const
{
    return d->sitesModel;
}

void OrganizationsModel::Private::clearCloudData()
{
    // Remove all except 'Sites' node.
    while (root->allChildren().size() > 1)
        remove(root->allChildren().size() - 1, root.get());
}

coro::Task<bool> OrganizationsModel::Private::loadOrgAsync(struct Organization org)
{
    auto groupsPath = nx::format(
        "/partners/api/v3/organizations/%1/groups_structure/",
        org.id.toSimpleStdString()).toStdString();
    auto systemsPath = nx::format(
        "/partners/api/v3/organizations/%1/cloud_systems/user_systems/",
        org.id.toSimpleStdString()).toStdString();

    auto [groupStructure, systems] = co_await nx::coro::whenAll(
        cloudGet<std::vector<GroupStructure>>(
            statusWatcher,
            groupsPath),
        cloudGet<SystemList>(
            statusWatcher,
            systemsPath,
            nx::UrlQuery()
                .addQueryItem("rootOnly", "false")
                .addQueryItem("showAllStatuses", "true")
                .addQueryItem("page_size", kMaximumPageSize)));

    if (groupStructure)
        setOrgStructure(org.id, *groupStructure);
    if (systems)
    {
        removeInactiveSystems(systems->results);
        setOrgSystems(org.id, systems->results);
    }

    if (groupStructure || systems)
    {
        if (auto node = nodes.find(org.id); node && node->loading)
        {
            node->loading = false;
            notifyNodeUpdate(node, {OrganizationsModel::IsLoadingRole});
        }
    }

    if (!groupStructure && groupStructure.error() != nx::cloud::db::api::ResultCode::ok)
    {
        NX_WARNING(this,
            "Error getting group structure for %1: %2",
            org.name,
            cloud::db::api::toString(groupStructure.error()));
        co_return false;
    }

    if (!systems && systems.error() != nx::cloud::db::api::ResultCode::ok)
    {
        NX_WARNING(this,
            "Error getting org systems for %1: %2",
            org.name,
            cloud::db::api::toString(systems.error()));
        co_return false;
    }

    co_return true;
}

coro::Task<bool> OrganizationsModel::Private::loadOrgListAsync(OrganizationList orgList)
{
    std::vector<nx::coro::Task<bool>> loadTasks;
    loadTasks.reserve(orgList.results.size());

    const auto isAccessible = [this](const auto& org) { return accessibleOrgs.contains(org.id); };

    for (const auto& org: orgList.results | std::views::filter(isAccessible))
        loadTasks.push_back(loadOrgAsync(org));

    auto results = co_await nx::coro::runAll(std::move(loadTasks), kMaxConcurrentRequests);
    if (std::any_of(results.begin(), results.end(), [](auto result) { return !result; }))
    {
        NX_WARNING(this, "Organizations are not fully loaded");
        co_return false;
    }

    co_return true;
}

coro::Task<bool> OrganizationsModel::Private::loadChannelPartnerOrgsAsync(ChannelPartnerList cpList)
{
    std::vector<nx::coro::Task<bool>> loadOrgTasks;

    for (const auto& cp: cpList.results)
    {
        loadOrgTasks.push_back(
            [](auto self, const auto& cp) -> coro::Task<bool>
            {
                auto cpOrgList = co_await cloudGet<OrganizationList>(
                    self->statusWatcher,
                    nx::format(
                        "/partners/api/v3/channel_partners/%1/organizations/",
                        cp.id.toSimpleStdString()).toStdString());

                if (!cpOrgList)
                {
                    NX_WARNING(self,
                        "Error getting organizations of %1: %2",
                        cp.id,
                        cloud::db::api::toString(cpOrgList.error()));
                    co_return false;
                }

                self->setOrganizations(cpOrgList->results, cp.id);

                // Show inaccessible organizations but don't load them.
                self->removeInaccessibleItems(&cpOrgList->results, cp);

                const bool result = co_await self->loadOrgListAsync(std::move(*cpOrgList));

                if (auto node = self->nodes.find(cp.id); node && node->loading)
                {
                    node->loading = false;
                    self->notifyNodeUpdate(node, {OrganizationsModel::IsLoadingRole});
                }

                co_return result;
            }(this, cp));
    }

    auto results = co_await nx::coro::runAll(std::move(loadOrgTasks), kMaxConcurrentRequests);

    co_return true;
}

coro::FireAndForget OrganizationsModel::Private::startPolling()
{
    ++updateGeneration;

    co_await nx::coro::cancelIf(
        [thisGeneration = updateGeneration, self = QPointer(q)]()
        {
            NX_ASSERT(qApp->thread() == QThread::currentThread());
            return !self || thisGeneration != self->d->updateGeneration;
        });

    setTopLevelLoading(statusWatcher->status() != CloudStatusWatcher::LoggedOut);
    const auto guard = nx::utils::ScopeGuard(
        nx::utils::guarded(
            q,
            [thisGeneration = updateGeneration, self = QPointer(q)]()
            {
                if (self && thisGeneration == self->d->updateGeneration)
                    self->d->setTopLevelLoading(false);
            }));

    for (;;)
    {
        if (!NX_ASSERT(statusWatcher))
            co_return;

        co_await coro::whenProperty(
            statusWatcher,
            "status",
            [](auto value) { return value == CloudStatusWatcher::Online; });

        // Load Partners/Organizations.
        // TODO: Channel structure should return groups and permissions.
        auto [channelPartnerStruct, channelPartnerList, orgList] = co_await coro::whenAll(
            cloudGet<ChannelStruct>(
                statusWatcher,
                "/partners/api/v3/channel_partners/channel_structure/"),
            cloudGet<ChannelPartnerList>(
                statusWatcher,
                "/partners/api/v3/channel_partners/"),
            cloudGet<OrganizationList>(
                statusWatcher,
                "/partners/api/v3/organizations/",
                nx::UrlQuery()
                    .addQueryItem("includeChildOrgs", "true"))
        );

        if (!channelPartnerStruct)
        {
            NX_WARNING(this,
                "Error getting channel structure: %1",
                cloud::db::api::toString(channelPartnerStruct.error()));
            co_await coro::qtTimer(kErrorRetryDelay);
            continue;
        }

        if (!channelPartnerList)
        {
            NX_WARNING(this,
                "Error getting channel partners: %1",
                cloud::db::api::toString(channelPartnerList.error()));
            co_await coro::qtTimer(kErrorRetryDelay);
            continue;
        }

        if (!orgList)
        {
            NX_WARNING(this,
                "Error getting organizations: %1",
                cloud::db::api::toString(orgList.error()));
            co_await coro::qtTimer(kErrorRetryDelay);
            continue;
        }

        // Fill caches with structs containing ownRolesIds/channelPartnerAccessLevel.
        QHash<nx::Uuid, nx::vms::client::core::ChannelPartner> channelPartnersCache;
        QHash<nx::Uuid, nx::vms::client::core::Organization> organizationsCache;

        for (const auto &partner: channelPartnerList->results)
            channelPartnersCache.insert(partner.id, partner);

        for (const auto &org: orgList->results)
            organizationsCache.insert(org.id, org);

        // Hide inaccessible channel partners.
        removeInaccessibleItems(&channelPartnerList->results);
        setChannelPartners(*channelPartnerList);

        // Fill in ownRolesIds/channelPartnerAccessLevel from the above cache as those fields are
        // missing from API response.
        fillOrgInfo(channelPartnerStruct->organizations, organizationsCache);

        // /v3/channel_partners/channel_structure/ does not privide a flat list of partners,
        // but contains organizations information. So make a list from /v3/channel_partners/
        // and fill partners organization lists.
        QHash<nx::Uuid, ChannelPartnerInStruct> visiblePartners;
        for (const auto &partner: channelPartnerList->results)
            visiblePartners[partner.id] = {};

        const auto fillOrgsRecursive =
            nx::utils::y_combinator([&organizationsCache, &visiblePartners](
                const auto fillOrgsRecursive,
                const ChannelPartnerInStruct& partner) -> void
            {
                if (auto it = visiblePartners.find(partner.id); it != visiblePartners.end())
                {
                    *it = partner;
                    fillOrgInfo(it->organizations, organizationsCache);
                }

                for (auto& subPartner: partner.subChannels)
                    fillOrgsRecursive(subPartner);
            });

        for (auto& partner: channelPartnerStruct->channelPartners)
            fillOrgsRecursive(partner);

        // Fill in organizations access cache.
        const auto isAccessible =
            [&channelPartnersCache](const nx::vms::client::core::Organization& org)
            {
                auto it = channelPartnersCache.find(org.channelPartner);
                return it != channelPartnersCache.end()
                    ? canAccess(org, it.value())
                    : canAccess(org);
            };

        accessibleOrgs.clear();
        for (const auto& org: orgList->results | std::views::filter(isAccessible))
            accessibleOrgs.insert(org.id);

        notifyNodeUpdate(
            root.get(),
            {OrganizationsModel::IsAccessibleThroughOrgRole},
            /*recursively*/ true);

        // Show inaccessible organizations but don't load them.
        setOrganizations(channelPartnerStruct->organizations, root->id);
        updateItemsAccess(channelPartnerStruct->organizations);

        // At this point we know all accessible channel partners and organizations and can show the
        // tab bar.
        setTopLevelLoading(false);

        // Set organizations for each channel partner.
        for (const auto& [partnerId, partner]: visiblePartners.asKeyValueRange())
        {
            if (partner.id.isNull())
                continue;

            setOrganizations(partner.organizations, partnerId);

            auto it = channelPartnersCache.find(partnerId);

            // Show inaccessible organizations but don't load them.
            if (it != channelPartnersCache.end())
                updateItemsAccess(partner.organizations, it.value());

            if (auto node = nodes.find(partnerId); node && node->loading)
            {
                node->loading = false;
                notifyNodeUpdate(node, {OrganizationsModel::IsLoadingRole});
            }
        }

        co_await loadOrgListAsync(std::move(*orgList));

        emit q->fullTreeLoaded();

        // Delay before next update.
        co_await coro::qtTimer(kUpdateDelay);
    }
}

bool OrganizationsModel::hasChannelPartners() const
{
    return d->hasChannelPartners;
}

void OrganizationsModel::Private::setHasChannelPartners(bool value)
{
    if (hasChannelPartners == value)
        return;

    hasChannelPartners = value;
    emit q->hasChannelPartnersChanged();
}

bool OrganizationsModel::hasOrganizations() const
{
    return d->hasOrganizations;
}

void OrganizationsModel::Private::setHasOrganizations(bool value)
{
    if (hasOrganizations == value)
        return;

    hasOrganizations = value;
    emit q->hasOrganizationsChanged();
}

QModelIndex OrganizationsModel::sitesRoot() const
{
    return d->sitesRoot();
}

bool OrganizationsModel::topLevelLoading() const
{
    return d->topLevelLoading;
}

void OrganizationsModel::setChannelPartners(const ChannelPartnerList& data)
{
    d->setChannelPartners(data);
}

void OrganizationsModel::setOrganizations(const OrganizationList& data, nx::Uuid parentId)
{
    d->setOrganizations(data.results, parentId);
}

void OrganizationsModel::setOrgStructure(nx::Uuid orgId, const std::vector<GroupStructure>& data)
{
    d->setOrgStructure(orgId, data);
}

void OrganizationsModel::setOrgSystems(
    nx::Uuid orgId,
    const std::vector<SystemInOrganization>& data)
{
    d->setOrgSystems(orgId, data);
}

void OrganizationsModel::setSystemsModel(QAbstractItemModel* systemsModel)
{
    if (d->sitesModel == systemsModel)
        return;

    d->connections.reset();

    if (systemsModel)
    {
        d->connections << connect(
            systemsModel,
            &QAbstractItemModel::dataChanged,
            this,
            [this](
                const QModelIndex& topLeft,
                const QModelIndex& bottomRight,
                const QVector<int>& roles)
            {
                emit dataChanged(
                    d->mapFromProxyModel(topLeft),
                    d->mapFromProxyModel(bottomRight),
                    roles);
            });

        d->connections << connect(systemsModel, &QAbstractItemModel::rowsAboutToBeInserted, this,
            [this](const QModelIndex& /*parent*/, int start, int end)
            {
                beginInsertRows(d->sitesRoot(), start, end);
            });

        d->connections << connect(systemsModel, &QAbstractItemModel::rowsInserted, this,
            [this](const QModelIndex& /*parent*/, int /*start*/, int /*end*/)
            {
                d->sitesModelRowCount = d->sitesModel->rowCount();
                endInsertRows();
            });

        d->connections << connect(systemsModel, &QAbstractItemModel::rowsAboutToBeRemoved, this,
            [this](const QModelIndex& /*parent*/, int start, int end)
            {
                beginRemoveRows(d->sitesRoot(), start, end);
            });

        d->connections << connect(systemsModel, &QAbstractItemModel::rowsRemoved, this,
            [this](const QModelIndex& /*parent*/, int /*start*/, int /*end*/)
            {
                d->sitesModelRowCount = d->sitesModel->rowCount();
                endRemoveRows();
            });

        d->connections << connect(systemsModel, &QAbstractItemModel::modelAboutToBeReset, this,
            [this]()
            {
                d->beginSitesReset();
            });

        d->connections << connect(systemsModel, &QAbstractItemModel::modelReset, this,
            [this]()
            {
                d->endSitesReset();
            });

        d->connections << connect(systemsModel, &QAbstractItemModel::layoutAboutToBeChanged, this,
            [this](
                const QList<QPersistentModelIndex>& parents,
                QAbstractItemModel::LayoutChangeHint hint)
            {
                NX_ASSERT(parents.empty(), "Sites model should be a list");
                if (hint == QAbstractItemModel::NoLayoutChangeHint)
                    d->beginSitesReset();
            });

        d->connections << connect(systemsModel, &QAbstractItemModel::layoutChanged, this,
            [this](
                const QList<QPersistentModelIndex>& parents,
                QAbstractItemModel::LayoutChangeHint hint)
            {
                NX_ASSERT(parents.empty(), "Sites model should be a list");
                if (hint == QAbstractItemModel::NoLayoutChangeHint)
                {
                    d->endSitesReset();
                }
                else if (d->sitesModelRowCount > 0)
                {
                    const auto sitesRoot = d->sitesRoot();
                    emit dataChanged(
                        index(0, 0, sitesRoot),
                        index(d->sitesModelRowCount - 1, 0, sitesRoot));
                }
            });

        d->connections << connect(systemsModel, &QAbstractItemModel::rowsMoved, this,
            [this](
                const QModelIndex& sourceParent, int sourceStart, int sourceEnd,
                const QModelIndex& destinationParent, int destinationRow)
            {
                NX_ASSERT(sourceParent == destinationParent);

                const int diff = sourceEnd - sourceStart;
                const int start = std::min(sourceStart, destinationRow);
                const int end = std::max(sourceEnd, destinationRow + diff);
                const auto sitesRoot = d->sitesRoot();
                emit dataChanged(
                    index(start, 0, sitesRoot),
                    index(end, 0, sitesRoot));
            });
    }

    const auto sitesRoot = d->sitesRoot();
    const int newRowCount = systemsModel ? systemsModel->rowCount() : 0;
    const int diff = newRowCount - d->sitesModelRowCount;

    if (diff > 0)
    {
        const ScopedInsertRows insertRows(
            this, sitesRoot, d->sitesModelRowCount, d->sitesModelRowCount + diff - 1);
        d->sitesModel = systemsModel;
        d->sitesModelRowCount = newRowCount;
    }
    else if (diff < 0)
    {
        const ScopedRemoveRows removeRows(
            this, sitesRoot, d->sitesModelRowCount + diff, d->sitesModelRowCount - 1);
        d->sitesModel = systemsModel;
        d->sitesModelRowCount = newRowCount;
    }
    else
    {
        d->sitesModel = systemsModel;
        d->sitesModelRowCount = newRowCount; //< No change, but added for consistency.
    }

    if (d->sitesModelRowCount > 0)
        emit dataChanged(index(0, 0, sitesRoot), index(d->sitesModelRowCount - 1, 0, sitesRoot));

    connect(this, &OrganizationsModel::hasChannelPartnersChanged, this,
        [this]() { d->notifyNodeUpdate(d->root.get(), {SectionRole}, /*recursively*/ true); });

    connect(this, &OrganizationsModel::hasOrganizationsChanged, this,
        [this]() { d->notifyNodeUpdate(d->root.get(), {SectionRole}, /*recursively*/ true); });
}

OrganizationsFilterModel::OrganizationsFilterModel(QObject* parent): base_type(parent)
{
    sort(0);
}

bool OrganizationsFilterModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex& sourceParent) const
{
    if (!sourceModel())
        return base_type::filterAcceptsRow(sourceRow, sourceParent);

    const auto sourceIndex = sourceModel()->index(sourceRow, 0);

    const auto nodeType =
        sourceIndex.data(OrganizationsModel::TypeRole).value<OrganizationsModel::NodeType>();

    if (nodeType == OrganizationsModel::SitesNode)
        return false;

    if (!m_showOnly.empty() && !m_showOnly.contains(nodeType))
        return false;

    if (m_hideOrgSystemsFromSites
        && sourceIndex.data(OrganizationsModel::IsFromSitesRole).toBool()
        && !sourceIndex.data(QnSystemsModel::IsSaasUninitialized).toBool()
        && sourceIndex.data(OrganizationsModel::IsAccessibleThroughOrgRole).toBool())
    {
        return false;
    }

    return base_type::filterAcceptsRow(sourceRow, sourceParent);
}

bool OrganizationsFilterModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const bool leftInCurrentRoot = isInCurrentRoot(left);
    const bool rightInCurrentRoot = isInCurrentRoot(right);
    if (leftInCurrentRoot != rightInCurrentRoot)
        return leftInCurrentRoot;

    // Other results.

    const bool leftFromSites = left.data(OrganizationsModel::IsFromSitesRole).toBool();
    const bool rightFromSites = right.data(OrganizationsModel::IsFromSitesRole).toBool();

    // 'Sites' section go last.
    if (leftFromSites != rightFromSites)
        return rightFromSites;

    const auto leftType = left.data(OrganizationsModel::TypeRole)
        .value<OrganizationsModel::NodeType>();
    const auto rightType = right.data(OrganizationsModel::TypeRole)
        .value<OrganizationsModel::NodeType>();

    // 'Partners' section go first.
    const bool leftIsPartner = leftType == OrganizationsModel::ChannelPartner;
    const bool rightIsPartner = rightType == OrganizationsModel::ChannelPartner;
    if (leftIsPartner != rightIsPartner)
        return leftIsPartner;

    if (!leftInCurrentRoot || !m_currentRoot.isValid())
    {
        // Other sections go in alphabetical order.
        const QString leftSection = left.data(OrganizationsModel::PathFromRootRole).toString();
        const QString rightSection = right.data(OrganizationsModel::PathFromRootRole).toString();

        const int sectionsOrder = nx::utils::naturalStringCompare(
            leftSection,
            rightSection,
            Qt::CaseInsensitive);

        if (sectionsOrder != 0)
            return sectionsOrder < 0;
    }

    if (leftType != rightType)
        return leftType < rightType;

    return QnSystemsModel::lessThan(left, right, /*cloudFirstSorting*/ true);
}

void OrganizationsFilterModel::setHideOrgSystemsFromSites(bool value)
{
    if (m_hideOrgSystemsFromSites == value)
        return;

    m_hideOrgSystemsFromSites = value;
    invalidateFilter();
    emit hideOrgSystemsFromSitesChanged();
}

void OrganizationsFilterModel::setShowOnly(const QList<OrganizationsModel::NodeType>& types)
{
    QSet<OrganizationsModel::NodeType> typesSet{types.begin(), types.end()};
    if (m_showOnly == typesSet)
        return;

    m_showOnly = typesSet;

    invalidateFilter();
    emit showOnlyChanged();
}

void OrganizationsFilterModel::setCurrentRoot(const QModelIndex& index)
{
    if (m_currentRoot == index)
        return;

    if (!m_currentRootWatcher)
    {
        m_currentRootWatcher = std::make_unique<PersistentIndexWatcher>();
        connect(m_currentRootWatcher.get(), &PersistentIndexWatcher::indexChanged,
            this, &OrganizationsFilterModel::invalidate);
        connect(m_currentRootWatcher.get(), &PersistentIndexWatcher::indexChanged,
            this, &OrganizationsFilterModel::currentRootChanged);
    }

    m_currentRoot = index;
    m_currentRootWatcher->setIndex(index);
}

QModelIndex OrganizationsFilterModel::currentRoot() const
{
    return m_currentRoot;
}

bool OrganizationsFilterModel::isInCurrentRoot(const QModelIndex& index) const
{
    if (!m_currentRoot.isValid())
    {
        auto tab = index.data(OrganizationsModel::TabSectionRole)
            .value<OrganizationsModel::TabSection>();

        return m_currentTab == tab;
    }

    if (!sourceModel())
        return true;

    auto listModel = qobject_cast<LinearizationListModel*>(sourceModel());
    if (!NX_ASSERT(listModel))
        return true;

    auto treeIndex = listModel->mapToSource(index.model() == this ? mapToSource(index) : index);

    if (treeIndex == m_currentRoot)
        return false;

    for (auto parent = treeIndex.parent(); parent.isValid(); parent = parent.parent())
    {
        if (parent == m_currentRoot)
            return true;
    }

    return false;
}

void OrganizationsFilterModel::setCurrentTab(OrganizationsModel::TabSection value)
{
    if (m_currentTab == value)
        return;
    m_currentTab = value;
    emit currentTabChanged();
    invalidate();
}

OrganizationsModel::TabSection OrganizationsFilterModel::currentTab() const
{
    return m_currentTab;
}

void OrganizationsFilterModel::sectionInfo(
    const QModelIndex& index, bool& otherResults, bool& addPlaceholder) const
{
    if (isInCurrentRoot(index))
    {
        otherResults = false;
        addPlaceholder = false;
        return;
    }

    if (index.row() == 0)
    {
        otherResults = true;
        addPlaceholder = true;
        return;
    }

    const auto sectionName = index.data(OrganizationsModel::PathFromRootRole).toString();

    for (int row = index.row() - 1; row >= 0; --row)
    {
        auto sibling = index.siblingAtRow(row);

        const bool inCurrentRoot = isInCurrentRoot(sibling);
        const bool sameSection = sectionName
            == sibling.data(OrganizationsModel::PathFromRootRole).toString();

        if (sameSection && !inCurrentRoot)
        {
            if (row != 0)
                continue;

            otherResults = true;
            addPlaceholder = true;
            return;
        }

        // We either found an item in the current root or we switched to another section.
        otherResults = inCurrentRoot; //< (sameSection && inCurrentRoot) || (!sameSection && inCurrentRoot)
        addPlaceholder = !inCurrentRoot && row == 0 && sameSection;
        return;
    }
}

QVariant OrganizationsFilterModel::data(const QModelIndex& index, int role) const
{
    if (!sourceModel())
        return base_type::data(index, role);

    if (role != OrganizationsModel::SectionRole)
        return base_type::data(index, role);

    if (isInCurrentRoot(index))
    {
        auto listModel = qobject_cast<LinearizationListModel*>(sourceModel());
        if (!NX_ASSERT(listModel))
            return {};

        auto treeIndex = listModel->mapToSource(mapToSource(index));

        if (treeIndex.parent() == m_currentRoot)
            return treeIndex.data(OrganizationsModel::SectionRole);
        else
            return index.data(OrganizationsModel::PathFromRootRole);
    }

    bool otherResults = false;
    bool addPlaceholder = false;

    // The first section which items are not under m_currentRoot should have additional title
    // "Other results". Placeholder for emptry results in current root is also part of this section.
    sectionInfo(index, otherResults, addPlaceholder);

    const QString prefix = otherResults
        ? (tr("Other results") + "\n") //< QML will split this on "\n".
        : QString{};

    const QString suffix = addPlaceholder
        ? QString("\n" + tr("Try changing the search parameters"))
        : QString{};

    return prefix + index.data(OrganizationsModel::PathFromRootRole).toString() + suffix;
}

} // nx::vms::client::core
