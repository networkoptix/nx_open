#pragma once

#include <QtCore/QSortFilterProxyModel>

#include <core/resource/resource_fwd.h>

class QnResourceListSortedModel: public QSortFilterProxyModel
{
    using base_type = QSortFilterProxyModel;
public:
    explicit QnResourceListSortedModel(QObject *parent = 0);
    virtual ~QnResourceListSortedModel();

    QnResourcePtr resource(const QModelIndex &index) const;

protected:
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

};
