#include "camera_list_model.h"

#include <QtCore/QUrlQuery>

#include "core/resource/camera_resource.h"
#include "core/resource/media_server_resource.h"
#include "core/resource_management/resource_pool.h"
#include "utils/common/id.h"
#include "utils/common/string.h"
#include "models/filtered_resource_list_model.h"
#include "camera/camera_thumbnail_cache.h"
#include "mobile_client/mobile_client_roles.h"
#include "api/network_proxy_factory.h"

namespace {
    class QnFilteredCameraListModel : public QnFilteredResourceListModel {
        typedef QnFilteredResourceListModel base_type;
    public:
        QnFilteredCameraListModel(QObject *parent = 0) :
            base_type(parent)
        {}

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
                        return QUrl(lit("qrc:///images/thumb_no_data.png"));

                    return QUrl(lit("image://thumbnail/") + thumbnailId);
                }
                return QUrl();
            }

            return base_type::data(index, role);
        }

    protected:
        virtual bool filterAcceptsResource(const QnResourcePtr &resource) const override {
            if (!resource->hasFlags(Qn::live_cam))
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
    m_model(new QnFilteredCameraListModel(this))
{
    setSourceModel(m_model);
    setDynamicSortFilter(true);
    sort(0);

    if (QnCameraThumbnailCache::instance())
        connect(QnCameraThumbnailCache::instance(), &QnCameraThumbnailCache::thumbnailUpdated, this, &QnCameraListModel::at_thumbnailUpdated);
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

void QnCameraListModel::at_thumbnailUpdated(const QnUuid &resourceId, const QString &thumbnailId) {
    Q_UNUSED(thumbnailId)
    m_model->refreshResource(qnResPool->getResourceById(resourceId), Qn::ThumbnailRole);
}
