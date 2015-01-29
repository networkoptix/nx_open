#include "server_list_model.h"

#include "core/resource/media_server_resource.h"
#include "utils/common/string.h"
#include "models/filtered_resource_list_model.h"

namespace {
    class QnFilteredServerListModel : public QnFilteredResourceListModel {
    public:
        QnFilteredServerListModel(QObject *parent = 0) :
            QnFilteredResourceListModel(parent)
        {}

    protected:
        virtual bool filterAcceptsResource(const QnResourcePtr &resource) const override {
            return !resource.dynamicCast<QnMediaServerResource>().isNull();
        }
    };
} // anonymous namespace

QnServerListModel::QnServerListModel(QObject *parent) :
    QSortFilterProxyModel(parent),
    m_model(new QnFilteredServerListModel(this))
{
    setSourceModel(m_model);
    setDynamicSortFilter(true);
    sort(0);
}

QnServerListModel::~QnServerListModel() {
}

bool QnServerListModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    QString leftName = left.data(Qn::ResourceNameRole).toString();
    QString rightName = right.data(Qn::ResourceNameRole).toString();

    return naturalStringLess(leftName, rightName);
}

