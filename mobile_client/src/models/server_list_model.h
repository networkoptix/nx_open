#ifndef QNSERVERLISTMODEL_H
#define QNSERVERLISTMODEL_H

#include <QtCore/QSortFilterProxyModel>

namespace {
    class QnFilteredServerListModel;
}

class QnServerListModel : public QSortFilterProxyModel {
public:
    QnServerListModel(QObject *parent = 0);
    ~QnServerListModel();

protected:
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    QnFilteredServerListModel *m_model;
};

#endif // QNSERVERLISTMODEL_H
