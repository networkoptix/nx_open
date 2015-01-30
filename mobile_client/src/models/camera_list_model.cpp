#include "camera_list_model.h"

#include "core/resource/security_cam_resource.h"
#include "core/resource_management/resource_pool.h"
#include "utils/common/id.h"
#include "utils/common/string.h"
#include "models/filtered_resource_list_model.h"
#include "camera/camera_thumbnail_cache.h"
#include "mobile_client/mobile_client_roles.h"

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
                        return QUrl();
                    return QUrl(lit("image://camera/") + thumbnailId);
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

bool QnCameraListModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    QString leftName = left.data(Qn::ResourceNameRole).toString();
    QString rightName = right.data(Qn::ResourceNameRole).toString();

    return naturalStringLess(leftName, rightName);
}

void QnCameraListModel::at_thumbnailUpdated(const QnUuid &resourceId, const QString &thumbnailId) {
    Q_UNUSED(thumbnailId)
    m_model->refreshResource(qnResPool->getResourceById(resourceId), Qn::ThumbnailRole);
}
