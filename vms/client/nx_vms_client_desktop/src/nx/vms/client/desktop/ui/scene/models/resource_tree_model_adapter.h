#pragma once

#include <ui/models/resource_tree_sort_proxy_model.h>

namespace nx::vms::client::desktop {

class ResourceTreeModelAdapter: public QnResourceTreeSortProxyModel
{
    Q_OBJECT
    using base_type = QnResourceTreeSortProxyModel;

public:
    ResourceTreeModelAdapter(QObject* parent = nullptr);

    enum class ItemState
    {
        normal,
        selected,
        accented
    };
    Q_ENUM(ItemState)

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    static void registerQmlType();
};

} // nx::vms::client::desktop
