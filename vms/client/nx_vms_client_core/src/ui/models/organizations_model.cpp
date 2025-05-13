// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "organizations_model.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaObject>

#include <nx/utils/coro/task_utils.h>
#include <nx/utils/coro/when_all.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/network/cloud_api.h>

namespace nx::vms::client::core {

namespace {

using namespace std::chrono;

static constexpr auto kErrorRetryDelay = 5s;
static constexpr auto kUpdateDelay = 20s;
static constexpr size_t kMaxConcurrentRequests = 10;

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

    nx::Uuid id;
    QString name;
    int systemCount = -1;
    OrganizationsModel::NodeType type = OrganizationsModel::None;
    bool loading = false;

private:
    TreeNode* parent = nullptr;
    std::vector<std::unique_ptr<TreeNode>> children;
    Map& idToNode;
};

} // namespace

struct OrganizationsModel::Private
{
    OrganizationsModel* const q;

    QPointer<CloudStatusWatcher> statusWatcher;
    QPointer<QAbstractProxyModel> proxyModel; //< Goes under special 'Sites' node.
    int proxyModelCount = 0;
    nx::utils::ScopedConnections connections;

    bool hasChannelPartners = false;
    bool hasOrganizations = false;

    TreeNode::Registry nodes; //< Should be destroyed after root.
    std::unique_ptr<TreeNode> root;

    bool topLevelLoading = false;
    uint64_t updateGeneration = 0;

    nx::utils::ScopedConnection statusConnection;
    CloudStatusWatcher::Status prevStatus = CloudStatusWatcher::LoggedOut;

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

        q->beginInsertRows(parentIndex, row, row);
        parent->insert(row, std::move(node));
        q->endInsertRows();
    }

    // Remove single node at index 'row' under 'parent'. Report model changes.
    void remove(int row, TreeNode* parent)
    {
        if (!parent)
            parent = root.get();

        auto parentIndex = parent == root.get()
            ? QModelIndex()
            : q->createIndex(parent->row(), 0, parent);

        q->beginRemoveRows(parentIndex, row, row);
        parent->remove(row);
        q->endRemoveRows();
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

        q->beginRemoveRows(parentIndex, 0, parent->allChildren().size() - 1);
        parent->clear();
        q->endRemoveRows();
    }

    void setChannelPartners(const ChannelPartnerList& data)
    {
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
            if (node->name != QString::fromStdString(partner.name))
            {
                node->name = QString::fromStdString(partner.name);
                auto cpIndex = q->createIndex(node->row(), 0, node);
                q->dataChanged(cpIndex, cpIndex);
            }
        }

        // Remove channel partners that are not present here.
        for (const auto& id: prevIds)
        {
            if (auto node = nodes.find(id))
                remove(node->row(), node->parentNode());
        }
    }

    void setOrganizations(const OrganizationList& data, nx::Uuid parentId)
    {
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
        for (const auto& org: data.results)
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
            if (node->name != QString::fromStdString(org.name)
                || node->systemCount != org.systemCount)
            {
                node->name = QString::fromStdString(org.name);
                node->systemCount = org.systemCount;
                auto orgIndex = q->createIndex(node->row(), 0, node);
                q->dataChanged(orgIndex, orgIndex);
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
    void setNodeGroups(
        TreeNode* parent,
        const std::vector<GroupStructure>& groups)
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
                        auto groupIndex = q->createIndex(node->row(), 0, node);
                        q->dataChanged(groupIndex, groupIndex);
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

        if (data.empty())
        {
            clear(orgNode);
            return;
        }

        setNodeGroups(orgNode, data);
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

        // Remove old systems
        std::unordered_set<nx::Uuid> oldIds;
        gatherSystems(orgNode, oldIds);

        for (const auto& system: data)
        {
            if (oldIds.contains(system.systemId))
                oldIds.erase(system.systemId); //< System is still present.
        }

        for (const auto& id: oldIds)
        {
            if (auto node = nodes.find(id))
                remove(node->row(), node->parentNode());
        }

        // Add new systems
        for (const auto& system: data)
        {
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
    }

    QModelIndex sitesRoot() const
    {
        return q->index(0, 0);
    }

    QModelIndex mapFromProxyModel(const QModelIndex& index) const
    {
        if (!proxyModel)
            return {};

        return q->index(index.row(), index.column(), sitesRoot());
    }

    QModelIndex mapToProxyModel(const QModelIndex& index) const
    {
        return proxyModel->index(index.row(), index.column());
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
        const int prevRowCount = proxyModel->rowCount();
        if (prevRowCount == 0)
            return;

        q->beginRemoveRows(sitesRoot(), 0, prevRowCount - 1);
        proxyModelCount = 0;
        q->endRemoveRows();
    }

    void endSitesReset()
    {
        const int newRowCount = proxyModel->rowCount();
        if (newRowCount == 0)
            return;

        q->beginInsertRows(sitesRoot(), 0, newRowCount - 1);
        proxyModelCount = newRowCount;
        q->endInsertRows();
    }

    void setHasChannelPartners(bool value);
    void setHasOrganizations(bool value);

    coro::FireAndForget startPolling();
    coro::Task<bool> loadOrgAsync(struct Organization org);
    coro::Task<bool> loadOrgListAsync(OrganizationList orgList);
    coro::Task<bool> loadChannelPartnerOrgsAsync(ChannelPartnerList cpList);
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
    if (d->proxyModel)
    {
        auto systemsModel = qobject_cast<QnSystemsModel*>(d->proxyModel->sourceModel());
        if (!systemsModel)
            return {};

        auto row = systemsModel->getRowIndex(id);
        if (row == -1)
            return {};

        return d->mapFromProxyModel(d->proxyModel->mapFromSource(systemsModel->index(row, 0)));
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

    // Do not use d->proxyModel->rowCount() directly as it requires special handling during model
    // reset.
    if (parentNode->type == OrganizationsModel::SitesNode)
        return d->proxyModelCount;

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
        if (!d->proxyModel)
            return {};

        if (role == SectionRole)
            return kSites;

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
            case QnSystemsModel::IsSaasUninitialized:
            {
                auto id = d->proxyModel->data(
                    d->mapToProxyModel(index),
                    QnSystemsModel::SystemIdRoleId).toString();
                // If offline system belongs to another channel partner we cannot detect SaaS state.
                // But at least we can detect that SaaS was initialized if the system belongs to
                // an organization in current channel partner.
                if (auto node = d->nodes.find(nx::Uuid::fromStringSafe(id)))
                    return false;
                break;
            }
        }
        return d->proxyModel->data(d->mapToProxyModel(index), role);
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
        case IsLoadingRole:
            return node->loading;
        case IsFromSitesRole:
            return false;
        case QnSystemsModel::IsSaasUninitialized:
            return node->type == SitesNode;
        default:
            if (node->type == OrganizationsModel::System && d->proxyModel)
            {
                auto systemsModel = qobject_cast<QnSystemsModel*>(d->proxyModel->sourceModel());
                if (!systemsModel)
                    return {};

                const int rowInSystems = systemsModel->getRowIndex(node->id.toSimpleString());
                if (rowInSystems == -1)
                    return {};

                return d->proxyModel->mapFromSource(
                    systemsModel->index(rowInSystems, 0)).data(role);
            }
            break;
    }
    return {};
}

QModelIndex OrganizationsModel::parent(const QModelIndex &index) const
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

QModelIndex OrganizationsModel::index(int row, int column, const QModelIndex &parent) const
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

bool OrganizationsModel::hasChildren(const QModelIndex &parent) const
{
    if (!d->root)
        return false;

    const TreeNode* parentNode = parent.isValid()
        ? static_cast<TreeNode*>(parent.internalPointer())
        : d->root.get();

    return parentNode && parentNode->type != OrganizationsModel::System;
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
        {IsFromSitesRole, "isFromSites"}
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
            }
            d->prevStatus = newStatus;
        },
        Qt::QueuedConnection));

    d->prevStatus = d->statusWatcher->status();

    if (d->prevStatus != CloudStatusWatcher::LoggedOut)
        d->startPolling();
}

QAbstractProxyModel* OrganizationsModel::systemsModel() const
{
    return d->proxyModel;
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
            nx::UrlQuery().addQueryItem("rootOnly", "false")));

    if (!groupStructure || !systems)
    {
        NX_WARNING(this,
            "Error getting group structure for %1: %2",
            org.name,
            cloud::db::api::toString(groupStructure.error()));
        co_return false;
    }

    setOrgStructure(org.id, *groupStructure);
    setOrgSystems(org.id, systems->results);

    if (auto node = nodes.find(org.id); node && node->loading)
    {
        node->loading = false;
        auto orgIndex = q->createIndex(node->row(), 0, node);
        q->dataChanged(orgIndex, orgIndex, {OrganizationsModel::IsLoadingRole});
    }

    co_return true;
}

coro::Task<bool> OrganizationsModel::Private::loadOrgListAsync(OrganizationList orgList)
{
    std::vector<nx::coro::Task<bool>> loadTasks;
    loadTasks.reserve(orgList.results.size());

    for (const auto& org: orgList.results)
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
            [](auto self, auto cpId) -> coro::Task<bool>
            {
                auto cpOrgList = co_await cloudGet<OrganizationList>(
                    self->statusWatcher,
                    nx::format(
                        "/partners/api/v3/channel_partners/%1/organizations/",
                        cpId.toSimpleStdString()).toStdString());

                if (!cpOrgList)
                {
                    NX_WARNING(self,
                        "Error getting organizations of %1: %2",
                        cpId,
                        cloud::db::api::toString(cpOrgList.error()));
                    co_return false;
                }

                self->setOrganizations(*cpOrgList, cpId);

                const bool result = co_await self->loadOrgListAsync(std::move(*cpOrgList));

                if (auto node = self->nodes.find(cpId); node && node->loading)
                {
                    node->loading = false;
                    auto orgIndex = self->q->createIndex(node->row(), 0, node);
                    self->q->dataChanged(
                        orgIndex, orgIndex, {OrganizationsModel::IsLoadingRole});
                }

                co_return result;
            }(this, cp.id));
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

        auto channelPartnerList = co_await cloudGet<ChannelPartnerList>(
            statusWatcher,
            "/partners/api/v3/channel_partners/");

        if (!channelPartnerList)
        {
            NX_WARNING(this,
                "Error getting channel partners: %1",
                cloud::db::api::toString(channelPartnerList.error()));
            co_await coro::qtTimer(kErrorRetryDelay);
            continue;
        }

        setHasChannelPartners(!channelPartnerList->results.empty());
        setChannelPartners(*channelPartnerList);

        if (!channelPartnerList->results.empty())
            co_await loadChannelPartnerOrgsAsync(std::move(*channelPartnerList));

        auto orgList = co_await cloudGet<OrganizationList>(
            statusWatcher,
            "/partners/api/v3/organizations/");

        if (!orgList)
        {
            NX_WARNING(this,
                "Error getting organizations: %1",
                cloud::db::api::toString(orgList.error()));
            co_await coro::qtTimer(kErrorRetryDelay);
            continue;
        }

        setOrganizations(*orgList, root->id);
        setHasOrganizations(!orgList->results.empty());
        setTopLevelLoading(false);

        co_await loadOrgListAsync(std::move(*orgList));

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

void OrganizationsModel::setSystemsModel(QAbstractProxyModel* systemsModel)
{
    if (d->proxyModel == systemsModel)
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
                d->proxyModelCount = d->proxyModel->rowCount();
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
                d->proxyModelCount = d->proxyModel->rowCount();
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
                else if (d->proxyModelCount > 0)
                {
                    const auto sitesRoot = d->sitesRoot();
                    emit dataChanged(
                        index(0, 0, sitesRoot),
                        index(d->proxyModelCount - 1, 0, sitesRoot));
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
    const int diff = newRowCount - d->proxyModelCount;

    if (diff > 0)
    {
        beginInsertRows(sitesRoot, d->proxyModelCount, d->proxyModelCount + diff - 1);
        d->proxyModel = systemsModel;
        d->proxyModelCount = newRowCount;
        endInsertRows();
    }
    else if (diff < 0)
    {
        beginRemoveRows(sitesRoot, d->proxyModelCount + diff, d->proxyModelCount - 1);
        d->proxyModel = systemsModel;
        d->proxyModelCount = newRowCount;
        endRemoveRows();
    }
    else
    {
        d->proxyModel = systemsModel;
        d->proxyModelCount = newRowCount; //< No change, but added for consistency.
    }

    if (d->proxyModelCount > 0)
        emit dataChanged(index(0, 0, sitesRoot), index(d->proxyModelCount - 1, 0, sitesRoot));
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
        && !sourceIndex.data(QnSystemsModel::IsSaasUninitialized).toBool())
    {
        return false;
    }

    return base_type::filterAcceptsRow(sourceRow, sourceParent);
}

bool OrganizationsFilterModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    QVariant leftParentType = left.parent().data(OrganizationsModel::TypeRole);
    QVariant rightParentType = right.parent().data(OrganizationsModel::TypeRole);

    if (leftParentType != rightParentType && leftParentType == OrganizationsModel::SitesNode)
        return false;

    QVariant leftType = left.data(OrganizationsModel::TypeRole);
    QVariant rightType = right.data(OrganizationsModel::TypeRole);

    if (leftType != rightType)
    {
        return leftType.value<OrganizationsModel::NodeType>()
            < rightType.value<OrganizationsModel::NodeType>();
    }

    return base_type::lessThan(left, right);
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

} // nx::vms::client::core
