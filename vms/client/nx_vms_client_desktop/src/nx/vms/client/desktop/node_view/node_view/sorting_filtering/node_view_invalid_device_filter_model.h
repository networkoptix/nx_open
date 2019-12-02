#pragma once

#include "node_view_base_sort_model.h"

// TODO: Move this model to the appropriate place, related to the resources.
namespace nx::vms::client::desktop {
namespace node_view {

class NodeViewInvalidDeviceFilterModel: public NodeViewBaseSortModel
{
    Q_OBJECT
    using base_type = NodeViewBaseSortModel;

public:
    NodeViewInvalidDeviceFilterModel(QObject* parent = nullptr);
    virtual ~NodeViewInvalidDeviceFilterModel() override;

public:
    void setShowInvalidDevices(bool showInvalidDevices);
    bool showInvalidDevices() const;

protected:
    virtual bool filterAcceptsRow(
        int sourceRow,
        const QModelIndex& sourceParent) const override;

private:
    bool m_showInvalidDevices = false;
};

} // namespace node_view
} // namespace nx::vms::client::desktop
