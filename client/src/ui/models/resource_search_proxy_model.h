#ifndef QN_RESOURCE_SEARCH_PROXY_MODEL_H
#define QN_RESOURCE_SEARCH_PROXY_MODEL_H

#include <QSortFilterProxyModel>
#include <core/resource/resource.h>

#if 0
class QnResourceSearchProxyModelPrivate;

class QnResourceSearchProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit QnResourceSearchProxyModel(QObject *parent = 0);
    virtual ~QnResourceSearchProxyModel();

    QnResourcePtr resourceFromIndex(const QModelIndex &index) const;
    QModelIndex indexFromResource(const QnResourcePtr &resource) const;

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;

private:
    Q_DISABLE_COPY(QnResourceSearchProxyModel)
        Q_DECLARE_PRIVATE(QnResourceSearchProxyModel)
        const QScopedPointer<QnResourceSearchProxyModelPrivate> d_ptr;
};
#endif

#endif //QN_RESOURCE_SEARCH_PROXY_MODEL_H
