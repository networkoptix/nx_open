#include "layouts_model.h"

#include <QtCore/QCoreApplication>

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <utils/common/connective.h>
#include <nx/utils/string.h>
#include <common/common_module.h>
#include <watchers/user_watcher.h>
#include <watchers/available_cameras_watcher.h>
#include <mobile_client/mobile_client_roles.h>

namespace {

struct ModelItem
{
    QnLayoutResourcePtr layout;
    QnMediaServerResourcePtr server;
    QnUuid id;

    ModelItem() {}

    ModelItem(const QnLayoutResourcePtr& layout) :
        layout(layout),
        id(layout ? layout->getId() : QnUuid())
    {}

    ModelItem(const QnMediaServerResourcePtr& server) :
        server(server),
        id(server ? server->getId() : QnUuid())
    {}

    QnLayoutsModel::ItemType type() const
    {
        if (layout)
            return QnLayoutsModel::ItemType::Layout;
        if (server)
            return QnLayoutsModel::ItemType::LiteClient;
        return QnLayoutsModel::ItemType::AllCameras;
    }

    QnResourcePtr resource() const
    {
        if (layout)
            return layout;
        return server;
    }

    QString name() const
    {
        return resource() ? resource()->getName() : QString();
    }

    bool isShared() const
    {
        return layout && layout->isShared();
    }
};

} // anonymous namespace

class QnLayoutsModelUnsorted : public Connective<QAbstractListModel>
{
    Q_DECLARE_TR_FUNCTIONS(QnLayoutsModelUnsorted)

    using base_type = Connective<QAbstractListModel>;

public:
    enum Roles
    {
        ItemTypeRole = Qn::RoleCount,
        IsSharedRole,
        ItemsCountRole,
        SectionRole
    };

    QnLayoutsModelUnsorted(QObject* parent = nullptr);

    virtual QHash<int, QByteArray> roleNames() const override;
    virtual int rowCount(const QModelIndex& parent) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;

private:
    void resetModel();
    bool isLayoutSuitable(const QnLayoutResourcePtr& layout) const;
    bool isServerSuitable(const QnMediaServerResourcePtr& server) const;

    void at_userChanged(const QnUserResourcePtr& user);
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);
    void at_resourceParentIdChanged(const QnResourcePtr& resource);
    void at_serverFlagsChanged(const QnResourcePtr& resource);

    QList<ModelItem> m_itemsList;
    QnUserResourcePtr m_user;
    int m_allCamerasCount;
};

QnLayoutsModelUnsorted::QnLayoutsModelUnsorted(QObject* parent):
    base_type(parent)
{
    const auto userWatcher = qnCommon->instance<QnUserWatcher>();
    connect(userWatcher, &QnUserWatcher::userChanged,
            this, &QnLayoutsModelUnsorted::at_userChanged);
    at_userChanged(userWatcher->user());

    const auto camerasWatcher = qnCommon->instance<QnAvailableCamerasWatcher>();
    auto updateCamerasCount = [this, camerasWatcher]()
    {
        m_allCamerasCount = camerasWatcher->availableCameras().size();
        const auto idx = index(0);
        emit dataChanged(idx, idx, QVector<int>{ItemsCountRole});
    };
    connect(camerasWatcher, &QnAvailableCamerasWatcher::cameraAdded,
            this, updateCamerasCount);
    connect(camerasWatcher, &QnAvailableCamerasWatcher::cameraRemoved,
            this, updateCamerasCount);

    connect(qnResPool, &QnResourcePool::resourceAdded,
            this, &QnLayoutsModelUnsorted::at_resourceAdded);
    connect(qnResPool, &QnResourcePool::resourceRemoved,
            this, &QnLayoutsModelUnsorted::at_resourceRemoved);
}

QHash<int, QByteArray> QnLayoutsModelUnsorted::roleNames() const
{
    auto roleNames = base_type::roleNames();
    roleNames[Qn::ResourceNameRole] = Qn::roleName(Qn::ResourceNameRole);
    roleNames[Qn::UuidRole] = Qn::roleName(Qn::UuidRole);
    roleNames[ItemTypeRole] = "itemType";
    roleNames[IsSharedRole] = "shared";
    roleNames[ItemsCountRole] = "itemsCount";
    roleNames[SectionRole] = "section";
    return roleNames;
}

int QnLayoutsModelUnsorted::rowCount(const QModelIndex& parent) const
{
    QN_UNUSED(parent);
    return m_itemsList.size();
}

QVariant QnLayoutsModelUnsorted::data(const QModelIndex& index, int role) const
{
    if (index.row() >= m_itemsList.size())
        return QVariant();

    const auto item = m_itemsList[index.row()];

    switch (role)
    {
        case Qn::ResourceRole:
            return QVariant::fromValue(item.resource());

        case Qn::LayoutResourceRole:
            return QVariant::fromValue(item.layout);

        case Qn::MediaServerResourceRole:
            return QVariant::fromValue(item.server);

        case Qn::ResourceNameRole:
            switch (item.type())
            {
                case QnLayoutsModel::ItemType::AllCameras:
                    return tr("All Cameras");
                default:
                    return item.name();
            }

        case Qn::UuidRole:
            return item.id.isNull() ? QString() : item.id.toString();

        case ItemTypeRole:
            return static_cast<int>(item.type());

        case IsSharedRole:
            return item.isShared();

        case ItemsCountRole:
            switch (item.type())
            {
                case QnLayoutsModel::ItemType::AllCameras:
                    return m_allCamerasCount;
                case QnLayoutsModel::ItemType::Layout:
                    return item.layout->getItems().size();
                default:
                    return -1;
            }

        case SectionRole:
            if (item.type() == QnLayoutsModel::ItemType::LiteClient)
                return static_cast<int>(QnLayoutsModel::ItemType::LiteClient);
            return static_cast<int>(QnLayoutsModel::ItemType::Layout);

        default:
            break;
    }

    return QVariant();
}

void QnLayoutsModelUnsorted::resetModel()
{
    beginResetModel();

    m_itemsList.clear();

    m_itemsList.append(ModelItem());

    const auto layouts = qnResPool->getResources<QnLayoutResource>();
    for (const auto& layout : layouts)
    {
        connect(layout, &QnLayoutResource::parentIdChanged,
                this, &QnLayoutsModelUnsorted::at_resourceParentIdChanged);

        if (!isLayoutSuitable(layout))
            continue;

        m_itemsList.append(layout);
    }

    if (qnResourceAccessManager->hasGlobalPermission(m_user, Qn::GlobalControlVideoWallPermission))
    {
        const auto servers = qnResPool->getResources<QnMediaServerResource>();
        for (const auto& server : servers)
        {
            connect(server, &QnMediaServerResource::serverFlagsChanged,
                    this, &QnLayoutsModelUnsorted::at_serverFlagsChanged);

            if (!isServerSuitable(server))
                continue;

            m_itemsList.append(server);
        }
    }

    endResetModel();
}

bool QnLayoutsModelUnsorted::isLayoutSuitable(const QnLayoutResourcePtr& layout) const
{
    if (!m_user)
        return false;

    const auto parentId = layout->getParentId();
    return parentId.isNull() || parentId == m_user->getId();
}

bool QnLayoutsModelUnsorted::isServerSuitable(const QnMediaServerResourcePtr& server) const
{
    return server->getServerFlags().testFlag(Qn::SF_HasLiteClient);
}

void QnLayoutsModelUnsorted::at_userChanged(const QnUserResourcePtr& user)
{
    if (m_user == user)
        return;

    m_user = user;
    resetModel();
}

void QnLayoutsModelUnsorted::at_resourceAdded(const QnResourcePtr& resource)
{
    const auto layout = resource.dynamicCast<QnLayoutResource>();
    if (!layout)
        return;

    connect(layout, &QnLayoutResource::parentIdChanged,
            this, &QnLayoutsModelUnsorted::at_resourceParentIdChanged);

    if (!isLayoutSuitable(layout))
        return;

    const auto row = m_itemsList.size();
    beginInsertRows(QModelIndex(), row, row);
    m_itemsList.append(layout);
    endInsertRows();
}

void QnLayoutsModelUnsorted::at_resourceRemoved(const QnResourcePtr& resource)
{
    const auto layout = resource.dynamicCast<QnLayoutResource>();
    if (!layout)
        return;

    disconnect(layout, nullptr, this, nullptr);

    auto it = std::find_if(m_itemsList.begin(), m_itemsList.end(),
                           [id = layout->getId()](const ModelItem& item)
                           {
                               return id == item.id;
                           }
    );
    if (it == m_itemsList.end())
        return;

    const auto row = std::distance(m_itemsList.begin(), it);
    beginRemoveRows(QModelIndex(), row, row);
    m_itemsList.erase(it);
    endRemoveRows();
}

void QnLayoutsModelUnsorted::at_resourceParentIdChanged(const QnResourcePtr& resource)
{
    const auto layout = resource.dynamicCast<QnLayoutResource>();
    if (!layout)
        return;

    bool suitable = isLayoutSuitable(layout);

    auto it = std::find_if(m_itemsList.begin(), m_itemsList.end(),
                           [id = layout->getId()](const ModelItem& item)
                           {
                               return id == item.id;
                           }
    );

    bool exists = it != m_itemsList.end();

    if (suitable == exists)
        return;

    if (suitable)
    {
        const auto row = m_itemsList.size();
        beginInsertRows(QModelIndex(), row, row);
        m_itemsList.append(layout);
        endInsertRows();
    }
    else
    {
        const auto row = std::distance(m_itemsList.begin(), it);
        beginRemoveRows(QModelIndex(), row, row);
        m_itemsList.erase(it);
        endRemoveRows();
    }
}

void QnLayoutsModelUnsorted::at_serverFlagsChanged(const QnResourcePtr& resource)
{
    QN_UNUSED(resource);
}

QnLayoutsModel::QnLayoutsModel(QObject* parent) :
    base_type(parent)
{
    setSourceModel(new QnLayoutsModelUnsorted(this));
    setDynamicSortFilter(true);
    sort(0);
}

bool QnLayoutsModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const auto leftType = left.data(QnLayoutsModelUnsorted::ItemTypeRole).value<ItemType>();
    const auto rightType = right.data(QnLayoutsModelUnsorted::ItemTypeRole).value<ItemType>();

    if (leftType != rightType)
        return leftType < rightType;

    // TODO: #dklychkov: Investigate why "return true" leads to "bad comparator":
    // debugging shows that in this case both leftType and rightType equal AllCameras.
    if (leftType == ItemType::AllCameras)
        return false;

    if (leftType == ItemType::Layout)
    {
        const auto leftLayout = left.data(Qn::LayoutResourceRole).value<QnLayoutResourcePtr>();
        const auto rightLayout = right.data(Qn::LayoutResourceRole).value<QnLayoutResourcePtr>();

        bool leftShared = leftLayout->isShared();
        bool rightShared = rightLayout->isShared();

        if (leftShared != rightShared)
            return leftShared;
    }

    const auto leftResource = left.data(Qn::ResourceRole).value<QnResourcePtr>();
    const auto rightResource = right.data(Qn::ResourceRole).value<QnResourcePtr>();

    const auto leftName = leftResource->getName();
    const auto rightName = rightResource->getName();

    auto cmp = nx::utils::naturalStringCompare(leftName, rightName, Qt::CaseInsensitive);
    if (cmp != 0)
        return cmp < 0;

    return leftResource->getId() < rightResource->getId();
}
