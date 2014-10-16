#ifndef SORT_FILTER_PLAIN_RESOURCE_MODEL_H
#define SORT_FILTER_PLAIN_RESOURCE_MODEL_H

#include <QtCore/QSortFilterProxyModel>

class QnSortFilterPlainResourceModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit QnSortFilterPlainResourceModel(QObject *parent = 0);

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
};

#endif // SORT_FILTER_PLAIN_RESOURCE_MODEL_H
