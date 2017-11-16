#pragma once

#include <QtCore/QVector>
#include <QtCore/QSortFilterProxyModel>

namespace nx {
namespace client {
namespace desktop {

// Proxy model that can be used to add/remove/rearrange columns in the source model.
// Data for added columns must be handled in subclasses (override flags(), data(), setData()).
// Model's inherited from QSortFilterProxyModel but cannot be used for rows sorting/filtering.
// TODO: #vkutin Inherit from QAbstractProxyModel and handle all source model signals.
class ColumnRemapProxyModel: public QSortFilterProxyModel
{
    Q_OBJECT
    using base_type = QSortFilterProxyModel;

public:
    // Constructor. Size of sourceColumns determines the model column count.
    // Indices in sourceColumns refer to source columns, -1 inserts a new not mapped column.
    explicit ColumnRemapProxyModel(
        const QVector<int>& sourceColumns,
        QObject* parent = nullptr);

    virtual ~ColumnRemapProxyModel() override;

    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    virtual QModelIndex index(int row, int column,
        const QModelIndex& parent = QModelIndex()) const override;
    virtual QModelIndex sibling(int row, int column, const QModelIndex& index) const override;
    virtual QModelIndex parent(const QModelIndex& index) const override;

    virtual QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;
    virtual QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;

    virtual void setSourceModel(QAbstractItemModel* sourceModel) override;

private:
    // Column mapping.
    const QVector<int> m_proxyToSource;
    QVector<int> m_sourceToProxy;
};

} // namespace desktop
} // namespace client
} // namespace nx
