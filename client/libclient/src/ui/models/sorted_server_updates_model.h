#ifndef SORTED_SERVER_UPDATES_MODEL_H
#define SORTED_SERVER_UPDATES_MODEL_H

#include <QtCore/QSortFilterProxyModel>

#include <ui/models/server_updates_model.h>

class QnSortedServerUpdatesModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit QnSortedServerUpdatesModel(QObject *parent = 0);

protected:
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
};

#endif // SORTED_SERVER_UPDATES_MODEL_H
