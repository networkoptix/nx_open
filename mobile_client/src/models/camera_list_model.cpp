#include "camera_list_model.h"

#include "core/resource/security_cam_resource.h"
#include "utils/common/id.h"
#include "utils/common/string.h"
#include "models/filtered_resource_list_model.h"

namespace {
    class QnFilteredCameraListModel : public QnFilteredResourceListModel {
    public:
        QnFilteredCameraListModel(QObject *parent = 0) :
            QnFilteredResourceListModel(parent)
        {}

        void setServerId(const QnUuid &id) {
            m_serverId = id;
            resetResources();
        }

        QnUuid serverId() const {
            return m_serverId;
        }

    protected:
        virtual bool filterAcceptsResource(const QnResourcePtr &resource) const override {
            if (resource.dynamicCast<QnSecurityCamResource>().isNull())
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
