#pragma once

#include <utils/common/connective.h>
#include <nx/vms/client/desktop/common/widgets/table_view.h>
#include <nx/utils/impl_ptr.h>

#include "../details/node_view_fwd.h"

namespace nx::vms::client::desktop {
namespace node_view {

class TableNodeView: public Connective<TableView>
{
    Q_OBJECT
    using base_type = Connective<TableView>;

public:
    TableNodeView(int columnCount, QWidget* parent = nullptr);
    virtual ~TableNodeView() override;

    void applyPatch(const details::NodeViewStatePatch& patch);

    const details::NodeViewState& state() const;

public:
    using HeaderDataProvider =
        std::function<bool (int section, Qt::Orientation orientation, int role, QVariant& data)>;
    void setHeaderDataProvider(HeaderDataProvider&& provider);

protected:
    const details::NodeViewStore& store() const;

    /**
     * Implements basic item check functionality.
     */
    virtual void handleDataChangeRequest(
        const QModelIndex& index,
        const QVariant& value,
        int role);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // node_view
} // namespace nx::vms::client::desktop
