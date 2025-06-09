// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layouts_model.h"

#include <QtCore/QCoreApplication>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/helpers/layout_item_aggregator.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/string.h>
#include <nx/utils/scoped_model_operations.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/qml/qml_ownership.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/client/mobile/system_context_aware.h>
#include <nx/vms/client/mobile/window_context.h>
#include <watchers/available_cameras_watcher.h>
#include <watchers/layout_cameras_watcher.h>

using namespace nx;
using namespace nx::core::access;
using namespace nx::vms::client::core;
using namespace nx::vms::client::mobile;

using nx::client::mobile::LayoutCamerasWatcher;

namespace {

struct ModelItem
{
    QnLayoutResourcePtr layout;
    nx::Uuid id;

    ModelItem() {}

    ModelItem(const QnLayoutResourcePtr& layout) :
        layout(layout),
        id(layout ? layout->getId() : nx::Uuid())
    {
    }

    QnLayoutsModel::ItemType type() const
    {
        if (!layout)
            return QnLayoutsModel::ItemType::AllCameras;

        return layout->hasFlags(Qn::cross_system)
            ? QnLayoutsModel::ItemType::CloudLayout
            : QnLayoutsModel::ItemType::Layout;
    }

    QnResourcePtr resource() const
    {
        return layout;
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

class QnLayoutsModelUnsorted:
    public ScopedModelOperations<QAbstractListModel>,
    public nx::vms::client::mobile::SystemContextAware
{
    Q_DECLARE_TR_FUNCTIONS(QnLayoutsModelUnsorted)
    using base_type = ScopedModelOperations<QAbstractListModel>;

public:
    enum Roles
    {
        ItemTypeRole = CoreItemDataRoleCount,
        IsSharedRole,
        ItemsCountRole,
        SectionRole
    };

    QnLayoutsModelUnsorted(nx::vms::client::mobile::SystemContext* context,
        QObject* parent = nullptr);

    virtual QHash<int, QByteArray> roleNames() const override;
    virtual int rowCount(const QModelIndex& parent) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;

private:
    void resetModel();
    bool isLayoutSuitable(const QnLayoutResourcePtr& layout) const;

    void at_userChanged(const QnUserResourcePtr& user);
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);
    void handleResourceAccessibilityChanged(const QnResourcePtr& resource);

    int itemRow(const nx::Uuid& id) const;
    void updateItem(const nx::Uuid& id);
    void addLayout(const QnLayoutResourcePtr& layout, bool duringReset = false);
    void removeLayout(const QnLayoutResourcePtr& layout);

    QList<ModelItem> m_itemsList;
    QHash<nx::Uuid, QSharedPointer<LayoutCamerasWatcher>> m_layoutCameraWatcherById;
    QnUserResourcePtr m_user;
    int m_allCamerasCount = 0;
};

QnLayoutsModelUnsorted::QnLayoutsModelUnsorted(nx::vms::client::mobile::SystemContext* context,
    QObject* parent)
    :
    base_type(parent),
    SystemContextAware(context)
{
    const auto userWatcher = systemContext()->userWatcher();
    connect(userWatcher, &nx::vms::client::core::UserWatcher::userChanged,
        this, &QnLayoutsModelUnsorted::at_userChanged);
    at_userChanged(userWatcher->user());

    const auto camerasWatcher = availableCamerasWatcher();
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

    connect(resourcePool(), &QnResourcePool::resourceAdded,
        this, &QnLayoutsModelUnsorted::at_resourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved,
        this, &QnLayoutsModelUnsorted::at_resourceRemoved);

    connect(appContext()->cloudLayoutsPool(), &QnResourcePool::resourceAdded,
        this, &QnLayoutsModelUnsorted::at_resourceAdded);
    connect(appContext()->cloudLayoutsPool(), &QnResourcePool::resourceRemoved,
        this, &QnLayoutsModelUnsorted::at_resourceRemoved);

    const auto handleAccessChanged =
        [this]()
        {
            const auto layouts = resourcePool()->getResources<QnLayoutResource>();
            for (const auto& layout: layouts)
                handleResourceAccessibilityChanged(layout);
        };

    connect(resourceAccessManager(), &QnResourceAccessManager::resourceAccessReset,
        this, handleAccessChanged);

    const auto notifier = resourceAccessManager()->createNotifier(this);
    connect(notifier, &QnResourceAccessManager::Notifier::resourceAccessChanged,
        this, handleAccessChanged);

    updateCamerasCount();
}

QHash<int, QByteArray> QnLayoutsModelUnsorted::roleNames() const
{
    auto roleNames = base_type::roleNames();
    roleNames.insert(clientCoreRoleNames());
    roleNames[ItemTypeRole] = "itemType";
    roleNames[IsSharedRole] = "shared";
    roleNames[ItemsCountRole] = "itemsCount";
    roleNames[SectionRole] = "section";
    return roleNames;
}

int QnLayoutsModelUnsorted::rowCount(const QModelIndex& /*parent*/) const
{
    return m_itemsList.size();
}

QVariant QnLayoutsModelUnsorted::data(const QModelIndex& index, int role) const
{
    if (index.row() >= m_itemsList.size())
        return QVariant();

    const auto item = m_itemsList[index.row()];

    switch (role)
    {
        case ResourceRole:
            return QVariant::fromValue(item.resource());

        case LayoutResourceRole:
            return QVariant::fromValue(item.layout);

        case ResourceNameRole:
            switch (item.type())
            {
                case QnLayoutsModel::ItemType::AllCameras:
                    return tr("All Cameras");
                default:
                    return item.name();
            }

        case RawResourceRole:
            return QVariant::fromValue(withCppOwnership(item.layout));

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
                case QnLayoutsModel::ItemType::CloudLayout:
                {
                    const auto watcher = m_layoutCameraWatcherById.value(item.id);
                    return watcher ? watcher->count() : 0;
                }

                default:
                    return -1;
            }

        case SectionRole:
            return static_cast<int>(QnLayoutsModel::ItemType::Layout);

        default:
            break;
    }

    return QVariant();
}

void QnLayoutsModelUnsorted::resetModel()
{
    const ScopedReset resetModel(this);

    m_itemsList.clear();

    m_itemsList.append(ModelItem());

    const auto layouts = resourcePool()->getResources<QnLayoutResource>();
    for (const auto& layout: layouts)
    {
        connect(layout.get(), &QnLayoutResource::parentIdChanged,
                this, &QnLayoutsModelUnsorted::handleResourceAccessibilityChanged);

        if (isLayoutSuitable(layout))
            addLayout(layout, /*duringReset*/ true);
    }

    const auto cloudLayouts = appContext()->cloudLayoutsPool()->getResources<QnLayoutResource>();
    for (const auto& cloudLayout: cloudLayouts)
        addLayout(cloudLayout, /*duringReset*/ true);
}

bool QnLayoutsModelUnsorted::isLayoutSuitable(const QnLayoutResourcePtr& layout) const
{
    if (!m_user || layout->isServiceLayout()
        || !resourceAccessManager()->hasPermission(m_user, layout, Qn::ReadPermission))
    {
        return false;
    }

    // We show only user's and shared layouts.
    return layout->isShared() || layout->getParentId() == m_user->getId();
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

    if (layout->hasFlags(Qn::cross_system))
    {
        addLayout(layout);
    }
    else
    {
        connect(layout.get(), &QnLayoutResource::parentIdChanged,
            this, &QnLayoutsModelUnsorted::handleResourceAccessibilityChanged);

        if (isLayoutSuitable(layout))
            addLayout(layout);
    }
}

void QnLayoutsModelUnsorted::at_resourceRemoved(const QnResourcePtr& resource)
{
    const auto layout = resource.dynamicCast<QnLayoutResource>();
    if (!layout)
        return;

    layout->disconnect(this);
    removeLayout(layout);
}

void QnLayoutsModelUnsorted::handleResourceAccessibilityChanged(const QnResourcePtr& resource)
{
    const auto layout = resource.dynamicCast<QnLayoutResource>();
    if (!layout)
        return;

    const bool suitable = isLayoutSuitable(layout);
    const bool exists = itemRow(layout->getId()) >= 0;

    if (suitable == exists)
        return;

    if (suitable)
        addLayout(layout);
    else
        removeLayout(layout);
}

int QnLayoutsModelUnsorted::itemRow(const nx::Uuid& id) const
{
    auto it = std::find_if(m_itemsList.begin(), m_itemsList.end(),
        [&id](const ModelItem& item) { return id == item.id; });

    if (it == m_itemsList.end())
        return -1;

    return std::distance(m_itemsList.begin(), it);
}

void QnLayoutsModelUnsorted::updateItem(const nx::Uuid& id)
{
    const auto row = itemRow(id);
    if (row < 0)
        return;

    const auto idx = index(row);
    emit dataChanged(idx, idx);
}

void QnLayoutsModelUnsorted::addLayout(const QnLayoutResourcePtr& layout, bool duringReset)
{
    const auto layoutId = layout->getId();

    if (const bool layoutExists = itemRow(layoutId) >= 0)
        return;

    const auto row = m_itemsList.size();

    const ScopedInsertRows insertRows(this, row, row, !duringReset);

    const auto watcher = QSharedPointer<LayoutCamerasWatcher>(new LayoutCamerasWatcher(this));
    watcher->setLayout(layout);
    m_layoutCameraWatcherById[layoutId] = watcher;

    m_itemsList.append(layout);

    connect(layout.get(), &QnLayoutResource::nameChanged, this,
        [this, layoutId]() { updateItem(layoutId); });

    connect(watcher.get(), &LayoutCamerasWatcher::countChanged, this,
        [this, layoutId]() { updateItem(layoutId); });
}

void QnLayoutsModelUnsorted::removeLayout(const QnLayoutResourcePtr& layout)
{
    const auto row = itemRow(layout->getId());
    if (row < 0)
        return;

    const ScopedRemoveRows removeRows(this, row, row);

    m_itemsList.removeAt(row);
    m_layoutCameraWatcherById.remove(layout->getId());

    disconnect(layout.get(), &QnLayoutResource::nameChanged, this, nullptr);
}

QnLayoutsModel::QnLayoutsModel(QObject* parent) :
    base_type(parent),
    nx::vms::client::mobile::WindowContextAware(
        nx::vms::client::mobile::WindowContext::fromQmlContext(this))
{
}

bool QnLayoutsModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const auto leftType = left.data(QnLayoutsModelUnsorted::ItemTypeRole).value<ItemType>();
    const auto rightType = right.data(QnLayoutsModelUnsorted::ItemTypeRole).value<ItemType>();

    // "All Cameras" is the first item.
    if (leftType == ItemType::AllCameras)
        return true;
    if (rightType == ItemType::AllCameras)
        return false;

    const auto leftResource = left.data(ResourceRole).value<QnResourcePtr>();
    const auto rightResource = right.data(ResourceRole).value<QnResourcePtr>();

    const auto leftName = leftResource->getName();
    const auto rightName = rightResource->getName();

    auto cmp = nx::utils::naturalStringCompare(leftName, rightName, Qt::CaseInsensitive);
    if (cmp != 0)
        return cmp < 0;

    return leftResource->getId() < rightResource->getId();
}

void QnLayoutsModel::onContextReady()
{
    setDynamicSortFilter(true);

    const auto updateModel =
        [this]()
        {
            const std::unique_ptr<QAbstractItemModel> previousSourceModelDeleter{sourceModel()};
            if (const auto systemContext = windowContext()->mainSystemContext())
            {
                setSourceModel(new QnLayoutsModelUnsorted(systemContext, this));
                sort(0);
            }
            else
            {
                setSourceModel(nullptr);
            }
        };

    updateModel();
    connect(windowContext(), &nx::vms::client::mobile::WindowContext::mainSystemContextChanged,
        this, updateModel);
}
