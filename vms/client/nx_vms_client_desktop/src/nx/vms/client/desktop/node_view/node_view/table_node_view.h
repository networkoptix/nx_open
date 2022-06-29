// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/common/widgets/table_view.h>
#include <nx/utils/impl_ptr.h>

#include "../details/node_view_fwd.h"

namespace nx::vms::client::desktop {
namespace node_view {

class TableNodeView: public TableView
{
    Q_OBJECT
    using base_type = TableView;

public:
    TableNodeView(int columnCount, QWidget* parent = nullptr);
    virtual ~TableNodeView() override;

    void applyPatch(const details::NodeViewStatePatch& patch);

    const details::NodeViewState& state() const;

public:
    using HeaderDataProvider =
        std::function<bool (int section, Qt::Orientation orientation, int role, QVariant& data)>;
    void setHeaderDataProvider(HeaderDataProvider&& provider);

    using IndicesList = QList<QModelIndex>;

    // The only supported user changes are checks. Generic changes should be added.
    IndicesList userCheckChangedIndices() const;

    void applyUserChanges();
    bool hasUserChanges() const;

    const details::NodeViewStorePtr& store();

signals:
    void hasUserChangesChanged() const;

protected:

    /**
     * Implements basic item check functionality.
     */
    virtual void handleUserDataChangeRequested(
        const QModelIndex& index,
        const QVariant& value,
        int role);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // node_view
} // namespace nx::vms::client::desktop
