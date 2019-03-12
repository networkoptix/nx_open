#pragma once

#include "node_view_base_sort_model.h"

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
    bool getShowInvalidDevices() const;

protected:
    virtual bool filterAcceptsRow(
        int sourceRow,
        const QModelIndex& sourceParent) const override;

private:
    bool m_showInvalidDevices = false;
};

} // namespace node_view
} // namespace nx::vms::client::desktop
