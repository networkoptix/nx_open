#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QScopedPointer>

#include <nx/client/desktop/common/models/abstract_mapping_model.h>
#include <nx/utils/scoped_model_operations.h>
#include <nx/utils/disconnect_helper.h>

namespace nx {
namespace client {
namespace desktop {

// SubsetListModel is a linear list model that proxies a single column
// from a single root node of another item model.

class SubsetListModel:
    public ScopedModelOperations<AbstractMappingModel<QAbstractListModel>>
{
    Q_OBJECT
    using base_type = ScopedModelOperations<AbstractMappingModel<QAbstractListModel>>;

public:
    explicit SubsetListModel(QObject* parent = nullptr);

    explicit SubsetListModel(
        QAbstractItemModel* sourceModel,
        int sourceColumn = 0,
        const QModelIndex& sourceRoot = QModelIndex(),
        QObject* parent = nullptr);

    virtual ~SubsetListModel() override;

    virtual QAbstractItemModel* sourceModel() const override;
    QModelIndex sourceRoot() const;
    int sourceColumn() const;

    void setSource(
        QAbstractItemModel* sourceModel, //< Ownership is not taken.
        int sourceColumn = 0,
        const QModelIndex& sourceRoot = QModelIndex());

    virtual QModelIndex mapToSource(const QModelIndex& index) const override;
    virtual QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    virtual bool insertRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
    virtual bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
    virtual bool moveRows(const QModelIndex& sourceParent, int sourceRow, int count,
        const QModelIndex& destinationParent, int destinationChild) override;

private:
    void sourceRowsAboutToBeInserted(const QModelIndex& parent, int first, int last);
    void sourceRowsInserted(const QModelIndex& parent);
    void sourceRowsAboutToBeRemoved(const QModelIndex& parent, int first, int last);
    void sourceRowsRemoved(const QModelIndex& parent);
    void sourceRowsAboutToBeMoved(const QModelIndex& sourceParent, int sourceFirst, int sourceLast,
        const QModelIndex& destinationParent, int destinationRow);
    void sourceRowsMoved(const QModelIndex& sourceParent, int sourceFirst, int sourceLast,
        const QModelIndex& destinationParent, int destinationRow);

    // If column insertion/removal/move occurs in the source model and affects the proxied column,
    // SubsetListModel::dataChanged signal is emitted for all rows in the model.

    void sourceColumnsInserted(const QModelIndex& parent, int first, int last);
    void sourceColumnsRemoved(const QModelIndex& parent, int first, int last);
    void sourceColumnsMoved(const QModelIndex& sourceParent, int sourceFirst, int sourceLast,
        const QModelIndex& destinationParent, int destinationColumn);

    // Source layout change with QAbstractItemModel::VerticalSortHint is fully proxied.
    // Source layout change with QAbstractItemModel::HorizontalSortHint is translated into
    //    dataChanged signal for all rows in the model.
    // Source layout change with QAbstractItemModel::NoLayoutChangeHint is translated into
    //    layoutAboutToBeChanged/layoutChanged and dataChanged signals.

    void sourceLayoutAboutToBeChanged(const QList<QPersistentModelIndex>& parents,
        QAbstractItemModel::LayoutChangeHint hint);
    void sourceLayoutChanged(const QList<QPersistentModelIndex>& parents,
        QAbstractItemModel::LayoutChangeHint hint);

    void connectToModel(QAbstractItemModel* model);
    void entireColumnChanged();

private:
    QAbstractItemModel* m_sourceModel = nullptr;
    int m_sourceColumn = 0;
    QPersistentModelIndex m_sourceRoot;
    QScopedPointer<QnDisconnectHelper> m_modelConnections;
    QVector<PersistentIndexPair> m_changingIndices;
};

} // namespace desktop
} // namespace client
} // namespace nx
