#include "node_view_invalid_device_filter_model.h"

#include <nx/vms/client/desktop/node_view/resource_node_view/resource_view_node_helpers.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_node_view_constants.h>

namespace nx::vms::client::desktop {
namespace node_view {

NodeViewInvalidDeviceFilterModel::NodeViewInvalidDeviceFilterModel(QObject* parent /*= nullptr*/):
    base_type(parent)
{
}

NodeViewInvalidDeviceFilterModel::~NodeViewInvalidDeviceFilterModel()
{
}

void NodeViewInvalidDeviceFilterModel::setShowInvalidDevices(bool show)
{
    if (m_isShowInvalidDevices == show)
        return;
    m_isShowInvalidDevices = show;
    invalidateFilter();
}

bool NodeViewInvalidDeviceFilterModel::isShowInvalidDevices() const
{
    return m_isShowInvalidDevices;
}

bool NodeViewInvalidDeviceFilterModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex& sourceParent) const
{
    if (!isShowInvalidDevices())
    {
        const auto sourceIndex = sourceModel()->index(
            sourceRow, ResourceNodeViewColumn::resourceNameColumn, sourceParent);

        auto hasOnlyInvalidChildren =
            [](const QModelIndex& index)
            {
                if (index.model()->rowCount(index) == 0)
                    return false;

                QModelIndexList childIndexes;
                for (int r = 0; r < index.model()->rowCount(index); ++r)
                {
                    childIndexes.append(index.model()->index(
                        r, ResourceNodeViewColumn::resourceNameColumn, index));
                }

                return std::all_of(childIndexes.cbegin(), childIndexes.cend(),
                    [](const QModelIndex& childIndex) { return !isValidNode(childIndex); });
            };

        if (!isValidNode(sourceIndex) || hasOnlyInvalidChildren(sourceIndex))
            return false;
    }
    return base_type::filterAcceptsRow(sourceRow, sourceParent);
}

} // namespace node_view
} // namespace nx::vms::client::desktop
