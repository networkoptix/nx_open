#include "camera_list_model.h"

#include <QtCore/QUrlQuery>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/string.h>
#include <models/available_camera_list_model.h>
#include <camera/camera_thumbnail_cache.h>
#include <mobile_client/mobile_client_roles.h>
#include <mobile_client/mobile_client_settings.h>

class QnCameraListModelPrivate: public QObject
{
public:
    QnCameraListModelPrivate();

    void at_thumbnailUpdated(const QnUuid& resourceId, const QString& thumbnailId);

public:
    QnAvailableCameraListModel* model;
};

QnCameraListModel::QnCameraListModel(QObject* parent) :
    base_type(parent),
    d_ptr(new QnCameraListModelPrivate())
{
    Q_D(QnCameraListModel);

    setSourceModel(d->model);
    setDynamicSortFilter(true);
    sort(0);
    setFilterCaseSensitivity(Qt::CaseInsensitive);

    auto cache = QnCameraThumbnailCache::instance();
    NX_ASSERT(cache);
    if (cache)
    {
        connect(cache, &QnCameraThumbnailCache::thumbnailUpdated,
                d, &QnCameraListModelPrivate::at_thumbnailUpdated);
    }

    connect(this, &QnCameraListModel::rowsInserted, this, &QnCameraListModel::countChanged);
    connect(this, &QnCameraListModel::rowsRemoved, this, &QnCameraListModel::countChanged);
    connect(this, &QnCameraListModel::modelReset, this, &QnCameraListModel::countChanged);
}

QnCameraListModel::~QnCameraListModel()
{
}

QHash<int, QByteArray> QnCameraListModel::roleNames() const
{
    auto roleNames = base_type::roleNames();
    roleNames[Qn::ThumbnailRole] = Qn::roleName(Qn::ThumbnailRole);
    return roleNames;
}

QVariant QnCameraListModel::data(const QModelIndex& index, int role) const
{
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    if (role != Qn::ThumbnailRole)
        return base_type::data(index, role);

    const auto cache = QnCameraThumbnailCache::instance();
    NX_ASSERT(cache);
    if (!cache)
        return QUrl();

    const auto id = QnUuid::fromStringSafe(base_type::data(index, Qn::UuidRole).toString());
    if (id.isNull())
        return QUrl();

    const auto thumbnailId = cache->thumbnailId(id);
    if (thumbnailId.isEmpty())
        return QUrl();

    return QUrl(lit("image://thumbnail/") + thumbnailId);
}

QString QnCameraListModel::layoutId() const
{
    Q_D(const QnCameraListModel);
    const auto layout = d->model->layout();
    return layout ? layout->getId().toString() : QString();
}

void QnCameraListModel::setLayoutId(const QString& layoutId)
{
    Q_D(QnCameraListModel);

    QnLayoutResourcePtr layout;

    const auto id = QnUuid::fromStringSafe(layoutId);
    if (!id.isNull())
        layout = resourcePool()->getResourceById<QnLayoutResource>(id);

    if (d->model->layout() == layout)
        return;

    d->model->setLayout(layout);
    emit layoutIdChanged();
}

int QnCameraListModel::count() const
{
    return rowCount();
}

int QnCameraListModel::rowByResourceId(const QString& resourceId) const
{
    Q_D(const QnCameraListModel);

    const auto id = QnUuid::fromStringSafe(resourceId);
    if (id.isNull())
        return -1;

    auto index = d->model->indexByResourceId(id);
    if (!index.isValid())
        return -1;

    index = mapFromSource(index);

    return index.row();
}

QString QnCameraListModel::resourceIdByRow(int row) const
{
    if (!hasIndex(row, 0))
        return QString();

    return data(index(row, 0), Qn::UuidRole).toString();
}

QString QnCameraListModel::nextResourceId(const QString& resourceId) const
{
    if (rowCount() == 0)
        return QString();

    if (resourceId.isEmpty())
        return resourceIdByRow(0);

    int row = rowByResourceId(resourceId);
    if (row == rowCount() - 1)
        return QString();

    return resourceIdByRow(row + 1);
}

QString QnCameraListModel::previousResourceId(const QString& resourceId) const
{
    if (rowCount() == 0)
        return QString();

    if (resourceId.isEmpty())
        return resourceIdByRow(rowCount() - 1);

    int row = rowByResourceId(resourceId);
    if (row == 0)
        return QString();

    return resourceIdByRow(row - 1);
}

void QnCameraListModel::refreshThumbnail(int row)
{
    if (!QnCameraThumbnailCache::instance())
        return;

    if (!hasIndex(row, 0))
        return;

    const auto id = data(index(row, 0), Qn::UuidRole).toUuid();
    QnCameraThumbnailCache::instance()->refreshThumbnail(id);
}

void QnCameraListModel::refreshThumbnails(int from, int to)
{
    if (!QnCameraThumbnailCache::instance())
        return;

    int rowCount = this->rowCount();

    if (from == -1)
        from = 0;

    if (to == -1)
        to = rowCount - 1;

    if (from >= rowCount || to >= rowCount || from > to)
        return;

    QList<QnUuid> ids;
    for (int i = from; i <= to; i++)
        ids.append(data(index(i, 0), Qn::UuidRole).toUuid());

    QnCameraThumbnailCache::instance()->refreshThumbnails(ids);
}

bool QnCameraListModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const auto leftName = left.data(Qn::ResourceNameRole).toString();
    const auto rightName = right.data(Qn::ResourceNameRole).toString();

    int res = nx::utils::naturalStringCompare(leftName, rightName, Qt::CaseInsensitive);
    if (res != 0)
        return res < 0;

    const auto leftAddress = left.data(Qn::IpAddressRole).toString();
    const auto rightAddress = right.data(Qn::IpAddressRole).toString();

    res = nx::utils::naturalStringCompare(leftAddress, rightAddress);
    if (res != 0)
        return res < 0;

    const auto leftId = left.data(Qn::UuidRole).toString();
    const auto rightId = right.data(Qn::UuidRole).toString();

    return leftId < rightId;
}

bool QnCameraListModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const auto index = sourceModel()->index(sourceRow, 0, sourceParent);
    const auto name = index.data(Qn::ResourceNameRole).toString();
    return name.contains(filterRegExp());
}

QnCameraListModelPrivate::QnCameraListModelPrivate() :
    model(new QnAvailableCameraListModel(this))
{
}

void QnCameraListModelPrivate::at_thumbnailUpdated(
    const QnUuid& resourceId, const QString& thumbnailId)
{
    Q_UNUSED(thumbnailId);

    model->refreshResource(model->resourcePool()->getResourceById(resourceId), Qn::ThumbnailRole);
}
