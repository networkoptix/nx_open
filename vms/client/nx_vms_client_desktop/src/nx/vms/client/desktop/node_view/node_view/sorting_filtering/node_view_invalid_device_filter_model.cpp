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

void NodeViewInvalidDeviceFilterModel::setShowInvalidDevices(bool showInvalidDevices)
{
    if (m_showInvalidDevices == showInvalidDevices)
        return;
    m_showInvalidDevices = showInvalidDevices;
    invalidateFilter();
}

bool NodeViewInvalidDeviceFilterModel::getShowInvalidDevices() const
{
    return m_showInvalidDevices;
}

bool NodeViewInvalidDeviceFilterModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex& sourceParent) const
{
    if (!getShowInvalidDevices())
    {
        const auto sourceIndex = sourceModel()->index(
            sourceRow, ResourceNodeViewColumn::resourceNameColumn, sourceParent);

        auto hasOnlyInvalidChildren =
            [](const QModelIndex& index)
            {
                auto model = index.model();

                if (model->rowCount(index) == 0)
                    return false;

                for (int row = 0; row < model->rowCount(index); ++row)
                {
                    auto childIndex =
                        model->index(row, ResourceNodeViewColumn::resourceNameColumn, index);
                    if (isValidNode(childIndex))
                        return false;
                }
                return true;
        };

        if (!isValidNode(sourceIndex) || hasOnlyInvalidChildren(sourceIndex))
            return false;
    }
    return base_type::filterAcceptsRow(sourceRow, sourceParent);
}

} // namespace node_view
} // namespace nx::vms::client::desktop
