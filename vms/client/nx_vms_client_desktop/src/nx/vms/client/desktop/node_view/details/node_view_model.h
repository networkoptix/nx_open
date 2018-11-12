#pragma once

#include <QtCore/QAbstractItemModel>

#include <nx/utils/scoped_model_operations.h>

#include "node_view_fwd.h"

namespace nx::vms::client::desktop {
namespace node_view {
namespace details {

class NodeViewModel: public ScopedModelOperations<QAbstractItemModel>
{
    Q_OBJECT
    using base_type = ScopedModelOperations<QAbstractItemModel>;

public:
    NodeViewModel(int columnCount, QObject* parent = nullptr);
    virtual ~NodeViewModel() override;

    void applyPatch(const NodeViewStatePatch& patch);

    QModelIndex index(const ViewNodePath& path, int column) const;

public: // Overrides section
    virtual QModelIndex index(
        int row,
        int column,
        const QModelIndex& parent = QModelIndex()) const override;

    virtual QModelIndex parent(const QModelIndex& child) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;

    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

signals:
    void dataChangeRequestOccured(const QModelIndex& index, const QVariant& value, int role);

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop

