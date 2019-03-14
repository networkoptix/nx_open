#pragma once

#include <QtCore/QSortFilterProxyModel>

#include <core/resource/resource_fwd.h>
#include <ui/models/resource/resource_compare_helper.h>

class QnResourceListSortedModel: public QSortFilterProxyModel, protected QnResourceCompareHelper
{
    using base_type = QSortFilterProxyModel;
public:
    explicit QnResourceListSortedModel(QObject* parent = nullptr);
    virtual ~QnResourceListSortedModel();

protected:
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
};
