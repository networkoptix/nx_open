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

        if (!isValidNode(sourceIndex))
            return false;
    }
    return base_type::filterAcceptsRow(sourceRow, sourceParent);
}

} // namespace node_view
} // namespace nx::vms::client::desktop
