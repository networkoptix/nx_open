#include "camera_list_model.h"

#include <QtCore/QUrlQuery>

#include "core/resource/camera_resource.h"
#include "core/resource/media_server_resource.h"
#include "core/resource_management/resource_pool.h"
#include "utils/common/id.h"
#include "utils/common/string.h"
#include "models/available_camera_list_model.h"
#include "camera/camera_thumbnail_cache.h"
#include "mobile_client/mobile_client_roles.h"
#include "mobile_client/mobile_client_settings.h"
#include "api/network_proxy_factory.h"

namespace {

    const qreal defaultAspectRatio = 4.0 / 3.0;

    class QnFilteredCameraListModel : public QnAvailableCameraListModel {
        typedef QnAvailableCameraListModel base_type;
    public:
        QnFilteredCameraListModel(QObject *parent = 0) :
            base_type(parent)
        {
            resetResourcesInternal();
        }

        void setServerId(const QnUuid &id) {
            m_serverId = id;
            resetResources();
        }

        QnUuid serverId() const {
            return m_serverId;
        }

        virtual QHash<int, QByteArray> roleNames() const override {
            QHash<int, QByteArray> roleNames = base_type::roleNames();
            roleNames[Qn::ThumbnailRole] = Qn::roleName(Qn::ThumbnailRole);
            return roleNames;
        }

        virtual QVariant data(const QModelIndex &index, int role) const override {
            if (!hasIndex(index.row(), index.column(), index.parent()))
                return QVariant();

            switch (role) {
            case Qn::ThumbnailRole:
                if (QnCameraThumbnailCache::instance()) {
                    QnUuid id = QnUuid::fromStringSafe(base_type::data(index, Qn::UuidRole).toString());
                    if (id.isNull())
                        return QUrl();

                    QString thumbnailId = QnCameraThumbnailCache::instance()->thumbnailId(id);
                    if (thumbnailId.isEmpty())
                        return QUrl();

                    return QUrl(lit("image://thumbnail/") + thumbnailId);
                }
                return QUrl();
            }

            return base_type::data(index, role);
        }

    protected:
        virtual bool filterAcceptsResource(const QnResourcePtr &resource) const override {
            bool accepted = base_type::filterAcceptsResource(resource);
            if (!accepted)
                return false;

            if (m_serverId.isNull())
                return true;

            return resource->getParentId() == m_serverId;
        }

    private:
        QnUuid m_serverId;
    };
} // anonymous namespace

QnCameraListModel::QnCameraListModel(QObject *parent) :
    QSortFilterProxyModel(parent),
    m_model(new QnFilteredCameraListModel(this)),
    m_showOffline(true),
    m_hiddenCamerasOnly(false)
{
    setSourceModel(m_model);
    setDynamicSortFilter(true);
    sort(0);
    setFilterCaseSensitivity(Qt::CaseInsensitive);

    if (QnCameraThumbnailCache::instance())
        connect(QnCameraThumbnailCache::instance(), &QnCameraThumbnailCache::thumbnailUpdated, this, &QnCameraListModel::at_thumbnailUpdated);
}

QHash<int, QByteArray> QnCameraListModel::roleNames() const {
    QHash<int, QByteArray> roles = QSortFilterProxyModel::roleNames();
    roles[Qn::ItemWidthRole] = "itemWidth";
    roles[Qn::ItemHeightRole] = "itemHeight";
    return roles;
}

QVariant QnCameraListModel::data(const QModelIndex &index, int role) const {
    if (role == Qn::ItemWidthRole || role == Qn::ItemHeightRole) {
        QnUuid id = QnUuid(QSortFilterProxyModel::data(index, Qn::UuidRole).toUuid());
        QSize size = m_sizeById.value(id);
        if (size.isEmpty())
            size = QSize(m_width / 2, m_width / 2 / defaultAspectRatio);

        if (role == Qn::ItemWidthRole)
            return size.width();
        else
            return size.height();
    }

    return QSortFilterProxyModel::data(index, role);
}

bool QnCameraListModel::showOffline() const {
    return m_showOffline;
}

void QnCameraListModel::setShowOffline(bool showOffline) {
    if (m_showOffline == showOffline)
        return;

    m_showOffline = showOffline;
    emit showOfflineChanged();

    invalidateFilter();
}

QStringList QnCameraListModel::hiddenCameras() const {
    return m_hiddenCameras.toList();
}

void QnCameraListModel::setHiddenCameras(const QStringList &hiddenCameras) {
    QSet<QString> hiddenCamerasSet = QSet<QString>::fromList(hiddenCameras);
    if (m_hiddenCameras == hiddenCamerasSet)
        return;

    m_hiddenCameras = hiddenCamerasSet;
    emit hiddenCamerasChanged();
    invalidateFilter();
}

bool QnCameraListModel::hiddenCamerasOnly() const {
    return m_hiddenCamerasOnly;
}

void QnCameraListModel::setHiddenCamerasOnly(bool hiddenCamerasOnly) {
    if (m_hiddenCamerasOnly == hiddenCamerasOnly)
        return;

    m_hiddenCamerasOnly = hiddenCamerasOnly;
    emit hiddenCamerasOnlyChanged();
    invalidateFilter();
}

void QnCameraListModel::setServerId(const QnUuid &id) {
    m_model->setServerId(id);
    emit serverIdStringChanged();
}

QnUuid QnCameraListModel::serverId() const {
    return m_model->serverId();
}

QString QnCameraListModel::serverIdString() const {
    return serverId().toString();
}

void QnCameraListModel::setServerIdString(const QString &id) {
    setServerId(QnUuid::fromStringSafe(id));
}

void QnCameraListModel::refreshThumbnail(int row) {
    if (!QnCameraThumbnailCache::instance())
        return;

    if (!hasIndex(row, 0))
        return;

    QnUuid id = QnUuid(data(index(row, 0), Qn::UuidRole).toUuid());
    QnCameraThumbnailCache::instance()->refreshThumbnails(QList<QnUuid>() << id);
}

void QnCameraListModel::refreshThumbnails(int from, int to) {
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
        ids.append(QnUuid(data(index(i, 0), Qn::UuidRole).toUuid()));

    QnCameraThumbnailCache::instance()->refreshThumbnails(ids);
}

void QnCameraListModel::updateLayout(int width, qreal desiredAspectRatio) {
    m_width = width;

    m_sizeById.clear();
    QnAspectRatioHash aspectRatios = qnSettings->camerasAspectRatios();

    int count = rowCount();
    for (int i = 0; i < count; ++i) {
        QnUuid id = QnUuid(index(i, 0).data(Qn::UuidRole).toUuid());
        qreal aspectRatio = aspectRatios.value(id, defaultAspectRatio);
        m_sizeById[id] = QSize(width / 2, width / 2 / aspectRatio);
    }

    emit dataChanged(index(0, 0), index(count - 1, 0), QVector<int>() << Qn::ItemWidthRole << Qn::ItemHeightRole);
}

bool QnCameraListModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    QString leftName = left.data(Qn::ResourceNameRole).toString();
    QString rightName = right.data(Qn::ResourceNameRole).toString();

    int res = naturalStringCompare(leftName, rightName, Qt::CaseInsensitive);
    if (res != 0)
        return res < 0;

    QString leftAddress = left.data(Qn::IpAddressRole).toString();
    QString rightAddress = right.data(Qn::IpAddressRole).toString();

    return naturalStringLess(leftAddress, rightAddress);
}

bool QnCameraListModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const {
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);

    if (!m_showOffline) {
        Qn::ResourceStatus status = static_cast<Qn::ResourceStatus>(index.data(Qn::ResourceStatusRole).toInt());
        if (status == Qn::Offline || status == Qn::NotDefined)
            return false;
    }

    QString id = index.data(Qn::UuidRole).toUuid().toString();
    if (m_hiddenCameras.contains(id))
        return m_hiddenCamerasOnly;
    else if (m_hiddenCamerasOnly)
        return false;

    QString name = index.data(Qn::ResourceNameRole).toString();
    return name.contains(filterRegExp());
}

void QnCameraListModel::at_thumbnailUpdated(const QnUuid &resourceId, const QString &thumbnailId) {
    m_model->refreshResource(qnResPool->getResourceById(resourceId), Qn::ThumbnailRole);

    QPixmap thumbnail = QnCameraThumbnailCache::instance()->getThumbnail(thumbnailId);
    if (!thumbnail.isNull()) {
        QnAspectRatioHash aspectRatios = qnSettings->camerasAspectRatios();
        aspectRatios[resourceId] = static_cast<qreal>(thumbnail.width()) / thumbnail.height();
        qnSettings->setCamerasAspectRatios(aspectRatios);
    }
}
