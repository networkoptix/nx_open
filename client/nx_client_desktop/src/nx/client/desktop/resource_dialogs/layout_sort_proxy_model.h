#pragma once

#include <nx/utils/uuid.h>
#include <nx/client/desktop/node_view/node_view/sorting/node_view_base_sort_model.h>

namespace nx {
namespace client {
namespace desktop {

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

} // namespace desktop
} // namespace client
} // namespace nx
