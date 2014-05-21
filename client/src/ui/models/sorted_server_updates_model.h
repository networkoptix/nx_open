#ifndef SORTED_SERVER_UPDATES_MODEL_H
#define SORTED_SERVER_UPDATES_MODEL_H

#include <QtCore/QSortFilterProxyModel>

#include <ui/models/server_updates_model.h>

class QnSortedServerUpdatesModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit QnSortedServerUpdatesModel(QObject *parent = 0);

    void setFilter(const QSet<QnId> &filter);

protected:
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QSet<QnId> m_filter;
};

#endif // SORTED_SERVER_UPDATES_MODEL_H
