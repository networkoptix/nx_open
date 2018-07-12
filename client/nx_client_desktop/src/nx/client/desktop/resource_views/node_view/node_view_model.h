#pragma once

#include <QtCore/QAbstractItemModel>

#include <nx/utils/scoped_model_operations.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>

namespace nx {
namespace client {
namespace desktop {

class NodeViewStore;
class NodeViewStatePatch;

class NodeViewModel: public ScopedModelOperations<QAbstractItemModel>
{
    Q_OBJECT
    using base_type = ScopedModelOperations<QAbstractItemModel>;

public:
    NodeViewModel(QObject* parent = nullptr);
    virtual ~NodeViewModel();

    void applyPatch(const NodeViewStatePatch& patch);

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

public:
    static NodePtr nodeFromIndex(const QModelIndex& index);

signals:
    void checkedChanged(const ViewNodePath& path, Qt::CheckState state);

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx

