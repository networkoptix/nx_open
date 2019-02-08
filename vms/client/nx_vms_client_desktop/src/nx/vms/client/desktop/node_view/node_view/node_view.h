#pragma once

#include <utils/common/connective.h>
#include <nx/vms/client/desktop/common/widgets/tree_view.h>

#include "../details/node_view_fwd.h"

class QSortFilterProxyModel;

namespace nx::vms::client::desktop {
namespace node_view {

class NodeView: public Connective<TreeView>
{
    Q_OBJECT
    using base_type = Connective<TreeView>;

public:
    NodeView(int columnCount, QWidget* parent = nullptr);
    virtual ~NodeView() override;

    void setProxyModel(QSortFilterProxyModel* proxy);

    void applyPatch(const details::NodeViewStatePatch& patch);

    const details::NodeViewState& state() const;

    void setHeightToContent(bool value);

    virtual void setupHeader();

protected:
    const details::NodeViewStore& store() const;
    details::NodeViewModel& sourceModel() const;

    /**
     * Implements basic item check functionality.
     */
    virtual void handleDataChangeRequest(
        const QModelIndex& index,
        const QVariant& value,
        int role);

private:
    // Node view uses special model and proxy. So we have to control this process.
    // Thus it is denied to set model directly. Please use setProxyModel to set any other proxy.
    virtual void setModel(QAbstractItemModel* model) override;

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // node_view
} // namespace nx::vms::client::desktop
