#include "layouts_model.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <utils/common/connective.h>
#include <utils/common/string.h>
#include <common/common_module.h>
#include <watchers/user_watcher.h>
#include <watchers/available_cameras_watcher.h>
#include <mobile_client/mobile_client_roles.h>

namespace {

struct ModelItem
{
    QnLayoutResourcePtr layout;
    QnUuid id;

    ModelItem(const QnLayoutResourcePtr& layout) :
        layout(layout),
        id(layout ? layout->getId() : QnUuid())
    {}

    QnLayoutResourcePtr layoutResource() const { return layout; }
    QString name() const { return layout ? layout->getName() : QString(); }
    bool isShared() const { return layout && layout->isShared(); }
};

} // anonymous namespace

class QnLayoutsModelUnsorted : public Connective<QAbstractListModel>
{
    using base_type = Connective<QAbstractListModel>;

public:
    enum Roles
    {
        IsSharedRole = Qn::RoleCount,
        ItemsCount
    };

    QnLayoutsModelUnsorted(QObject* parent = nullptr);

    virtual QHash<int, QByteArray> roleNames() const override;
    virtual int rowCount(const QModelIndex& parent) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;

private:
    void resetModel();
    bool isLayoutSuitable(const QnLayoutResourcePtr& layout) const;

    void at_userChanged(const QnUserResourcePtr& user);
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);
    void at_resourceParentIdChanged(const QnResourcePtr& resource);

    QList<ModelItem> m_layoutsList;
    QnUuid m_userId;
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
    roleNames[IsSharedRole] = "shared";
    roleNames[ItemsCount] = "itemsCount";
    return roleNames;
}

int QnLayoutsModelUnsorted::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return m_layoutsList.size();
}

QVariant QnLayoutsModelUnsorted::data(const QModelIndex& index, int role) const
{
    if (index.row() >= m_layoutsList.size())
        return QVariant();

    const auto item = m_layoutsList[index.row()];

    switch (role)
    {
        case Qn::LayoutResourceRole:
            return QVariant::fromValue(item.layout);
        case Qn::ResourceNameRole:
            return item.layout ? item.name() : tr("All Cameras");
        case Qn::UuidRole:
            return item.id.isNull() ? QString() : item.id.toString();
        case IsSharedRole:
            return item.isShared();
        case ItemsCount:
            return item.layout ? item.layout->getItems().size() : m_allCamerasCount;
        default:
            break;
    }

    return QVariant();
}

void QnLayoutsModelUnsorted::resetModel()
{
    beginResetModel();

    m_layoutsList.clear();
    m_layoutsList.append(QnLayoutResourcePtr());
    const auto layouts = qnResPool->getResources<QnLayoutResource>();
    for (const auto& layout : layouts)
    {
        connect(layout, &QnLayoutResource::parentIdChanged,
                this, &QnLayoutsModelUnsorted::at_resourceParentIdChanged);

        if (!isLayoutSuitable(layout))
            continue;

        m_layoutsList.append(layout);
    }

    endResetModel();
}

bool QnLayoutsModelUnsorted::isLayoutSuitable(const QnLayoutResourcePtr& layout) const
{
    const auto parentId = layout->getParentId();
    return parentId.isNull() || parentId == m_userId;
}

void QnLayoutsModelUnsorted::at_userChanged(const QnUserResourcePtr& user)
{
    const auto id = user ? user->getId() : QnUuid();
    if (m_userId == id)
        return;

    m_userId = id;
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

    const auto row = m_layoutsList.size();
    beginInsertRows(QModelIndex(), row, row);
    m_layoutsList.append(layout);
    endInsertRows();
}

void QnLayoutsModelUnsorted::at_resourceRemoved(const QnResourcePtr& resource)
{
    const auto layout = resource.dynamicCast<QnLayoutResource>();
    if (!layout)
        return;

    disconnect(layout, nullptr, this, nullptr);

    auto it = std::find_if(m_layoutsList.begin(), m_layoutsList.end(),
                           [id = layout->getId()](const auto& item)
                           {
                               return id == item.id;
                           }
    );
    if (it == m_layoutsList.end())
        return;

    const auto row = std::distance(m_layoutsList.begin(), it);
    beginRemoveRows(QModelIndex(), row, row);
    m_layoutsList.erase(it);
    endRemoveRows();
}

void QnLayoutsModelUnsorted::at_resourceParentIdChanged(const QnResourcePtr& resource)
{
    const auto layout = resource.dynamicCast<QnLayoutResource>();
    if (!layout)
        return;

    bool suitable = isLayoutSuitable(layout);

    auto it = std::find_if(m_layoutsList.begin(), m_layoutsList.end(),
                           [id = layout->getId()](const auto& item)
                           {
                               return id == item.id;
                           }
    );

    bool exists = it != m_layoutsList.end();

    if (suitable == exists)
        return;

    if (suitable)
    {
        const auto row = m_layoutsList.size();
        beginInsertRows(QModelIndex(), row, row);
        m_layoutsList.append(layout);
        endInsertRows();
    }
    else
    {
        const auto row = std::distance(m_layoutsList.begin(), it);
        beginRemoveRows(QModelIndex(), row, row);
        m_layoutsList.erase(it);
        endRemoveRows();
    }
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
    const auto leftLayout = left.data(Qn::LayoutResourceRole).value<QnLayoutResourcePtr>();
    const auto rightLayout = right.data(Qn::LayoutResourceRole).value<QnLayoutResourcePtr>();

    const auto leftId = leftLayout ? leftLayout->getId() : QnUuid();
    if (leftId.isNull())
        return true;

    const auto rightId = rightLayout ? rightLayout->getId() : QnUuid();
    if (rightId.isNull())
        return false;

    bool leftShared = leftLayout->isShared();
    bool rightShared = rightLayout->isShared();

    if (leftShared != rightShared)
        return leftShared;

    const auto leftName = leftLayout->getName();
    const auto rightName = rightLayout->getName();

    auto cmp = naturalStringCompare(leftName, rightName, Qt::CaseInsensitive);
    if (cmp != 0)
        return cmp < 0;

    return leftId < rightId;
}
