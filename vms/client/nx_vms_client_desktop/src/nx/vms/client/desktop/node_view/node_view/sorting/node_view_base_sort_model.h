#pragma once

#include <QtCore/QSortFilterProxyModel>

namespace nx::vms::client::desktop {
namespace node_view {

class NodeViewBaseSortModel: public QSortFilterProxyModel
{
    Q_OBJECT
    using base_type = QSortFilterProxyModel;

public:
    NodeViewBaseSortModel(QObject* parent = nullptr);

    virtual ~NodeViewBaseSortModel() override;

    virtual void setSourceModel(QAbstractItemModel* model) override;

    enum FilterScope
    {
        AnyNodeFilterScope,
        LeafNodeFilterScope
    };

    void setFilter(const QString& filter, FilterScope scope = AnyNodeFilterScope);

protected:
    bool nextLessThan(
        const QModelIndex& sourceLeft,
        const QModelIndex& sourceRight) const;

    virtual bool filterAcceptsRow(
        int sourceRow,
        const QModelIndex& sourceParent) const override;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace node_view
} // namespace nx::vms::client::desktop
