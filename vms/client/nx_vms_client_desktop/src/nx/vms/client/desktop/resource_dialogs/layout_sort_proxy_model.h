#pragma once

#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/node_view/node_view/sorting_filtering/node_view_base_sort_model.h>

namespace nx::vms::client::desktop {

class LayoutSortProxyModel: public node_view::NodeViewBaseSortModel
{
    using base_type = node_view::NodeViewBaseSortModel;

public:
    LayoutSortProxyModel(const QnUuid& currentUserId, QObject* parent = nullptr);

protected:
    virtual bool lessThan(
        const QModelIndex& sourceLeft,
        const QModelIndex& sourceRight) const;

private:
    const QnUuid m_currentUserId;
};

} // namespace nx::vms::client::desktop
