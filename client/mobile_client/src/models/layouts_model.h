#pragma once

#include <QtCore/QSortFilterProxyModel>

class QnLayoutsModel : public QSortFilterProxyModel
{
    using base_type = QSortFilterProxyModel;

public:
    QnLayoutsModel(QObject* parent = nullptr);

    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};
