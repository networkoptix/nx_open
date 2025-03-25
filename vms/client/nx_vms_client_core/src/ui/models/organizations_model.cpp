// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "organizations_model.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaObject>

#include <nx/network/http/rest/http_rest_client.h>
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

    nx::Uuid channelPartner;

    TreeNode::Registry nodes; //< Should be destroyed after root.
    std::unique_ptr<TreeNode> root;

    bool channelPartnerLoading = false;
    uint64_t updateGeneration = 0;

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

    void setChannelPartnerOrgs(const OrganizationList& data)
    {
        std::unordered_set<nx::Uuid> prevIds;
        for (const auto& node: root->allChildren())
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
                    root->allChildren().size(),
                    nodes.create(org),
                    root.get());
                continue;
            }
            prevIds.erase(org.id);
            auto node = nodes.find(org.id);
            if (node->name != QString::fromStdString(org.name)
                || node->systemCount != org.systemCount)
            {
                node->name = QString::fromStdString(org.name);
                node->systemCount = org.systemCount;
                auto orgIndex = q->createIndex(node->row(), 0, node->parentNode());
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
                        auto groupIndex = q->createIndex(node->row(), 0, parent);
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

    nx::coro::FireAndForget loadOrgs(nx::Uuid cpId);

    void setChannelPartnerLoading(bool loading)
    {
        if (channelPartnerLoading == loading)
            return;

        channelPartnerLoading = loading;
        emit q->channelPartnerLoadingChanged();
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
};

nx::coro::FireAndForget OrganizationsModel::Private::loadOrgs(nx::Uuid cpId)
{
    co_await nx::coro::cancelIf(
        [thisGeneration = updateGeneration, self = QPointer(q)]()
        {
            NX_ASSERT(qApp->thread() == QThread::currentThread());
            return !self || thisGeneration != self->d->updateGeneration;
        });

    setChannelPartnerLoading(true);
    auto g = nx::utils::ScopeGuard(
        nx::utils::guarded(q, [this]() { setChannelPartnerLoading(false); }));

    for (;;)
    {
        if (!NX_ASSERT(statusWatcher))
            co_return;

        co_await coro::whenProperty(
            statusWatcher,
            "status",
            [](auto value) { return value == CloudStatusWatcher::Online; });

        auto orgsPath = nx::network::http::rest::substituteParameters(
            "/partners/api/v2/channel_partners/{id}/organizations/",
            {cpId.toSimpleStdString()});
        auto orgList = co_await cloudGet<OrganizationList>(
            statusWatcher,
            orgsPath);

        if (!orgList)
        {
            NX_WARNING(this,
                "Error getting organizations of %1: %2",
                cpId,
                cloud::db::api::toString(orgList.error()));
            co_await coro::qtTimer(kErrorRetryDelay);
            continue;
        }

        setChannelPartnerOrgs(*orgList);
        setChannelPartnerLoading(false);

        std::vector<nx::coro::Task<bool>> loadTasks;
        loadTasks.reserve(orgList->results.size());

        for (const auto& org: orgList->results)
        {
            loadTasks.push_back(
                [](auto self, auto org) -> nx::coro::Task<bool>
                {
                    auto groupsPath = nx::network::http::rest::substituteParameters(
                        "/partners/api/v2/organizations/{id}/groups_structure/",
                        {org.id.toSimpleStdString()});
                    auto systemsPath = nx::network::http::rest::substituteParameters(
                        "/partners/api/v2/organizations/{id}/cloud_systems/user_systems/",
                        {org.id.toSimpleStdString()});

                    auto [groupStructure, systems] = co_await nx::coro::whenAll(
                        cloudGet<std::vector<GroupStructure>>(
                            self->statusWatcher,
                            groupsPath),
                        cloudGet<SystemList>(
                            self->statusWatcher,
                            systemsPath,
                            nx::utils::UrlQuery().addQueryItem("rootOnly", "false")));

                    if (!groupStructure || !systems)
                    {
                        if (!groupStructure)
                        {
                            NX_WARNING(self,
                                "Error getting group structure for %1: %2",
                                org.name,
                                cloud::db::api::toString(groupStructure.error()));
                        }
                        if (!systems)
                        {
                            NX_WARNING(self,
                                "Error getting systems for %1: %2",
                                org.name,
                                cloud::db::api::toString(systems.error()));
                        }
                        co_return false;
                    }

                    self->setOrgStructure(org.id, *groupStructure);
                    self->setOrgSystems(org.id, systems->results);

                    if (auto node = self->nodes.find(org.id); node->loading)
                    {
                        node->loading = false;
                        auto orgIndex = self->q->createIndex(node->row(), 0, node->parentNode());
                        self->q->dataChanged(
                            orgIndex, orgIndex, {OrganizationsModel::IsLoadingRole});
                    }

                    co_return true;
                }(this, org));
        }

        auto results = co_await nx::coro::runAll(std::move(loadTasks), kMaxConcurrentRequests);

        if (std::any_of(results.begin(), results.end(), [](auto result) { return !result; }))
        {
            NX_WARNING(this, "Organizations are not fully loaded");
            co_await coro::qtTimer(kErrorRetryDelay);
            continue;
        }

        // Delay before next update.
        co_await coro::qtTimer(kUpdateDelay);
    }
}

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

    // Do not use d->proxyModel->rowCount() directly as it requires specias handling during model
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
            if (node->type == OrganizationsModel::Organization)
                return kOrganizations;
            if (node->type == OrganizationsModel::Folder)
                return kFolders;
            if (node->type == OrganizationsModel::System)
                return kSites;
            return "";
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

    d->statusWatcher = statusWatcher;
    emit statusWatcherChanged();
}

QAbstractProxyModel* OrganizationsModel::systemsModel() const
{
    return d->proxyModel;
}

nx::Uuid OrganizationsModel::channelPartner() const
{
    return d->channelPartner;
}

void OrganizationsModel::setChannelPartner(nx::Uuid channelPartner)
{
    if (d->channelPartner == channelPartner)
        return;

    d->channelPartner = channelPartner;
    emit channelPartnerChanged();

    d->updateGeneration++;

    // Remove all top level nodes (organizations) except the 'Sites' node.
    while (d->root->allChildren().size() > 1)
        d->remove(1, d->root.get());

    if (!channelPartner.isNull())
        d->loadOrgs(channelPartner);
}

QModelIndex OrganizationsModel::sitesRoot() const
{
    return d->sitesRoot();
}

bool OrganizationsModel::channelPartnerLoading() const
{
    return d->channelPartnerLoading;
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
    if (sourceIndex.data(OrganizationsModel::TypeRole).value<OrganizationsModel::NodeType>()
        == OrganizationsModel::SitesNode)
    {
        return false;
    }

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

} // nx::vms::client::core
