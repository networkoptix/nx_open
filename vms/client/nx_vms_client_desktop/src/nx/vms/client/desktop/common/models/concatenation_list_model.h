// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <initializer_list>

#include <QtCore/QAbstractListModel>
#include <QtCore/QScopedPointer>

#include <nx/vms/client/desktop/common/models/abstract_mapping_model.h>
#include <nx/utils/scoped_model_operations.h>
#include <nx/utils/scoped_connections.h>

namespace nx::vms::client::desktop {

// ConcatenationListModel is a linear list model that concatenates several other list models.
// It is not optimized for use with large numbers of source models.

// Proxies everything except:
//   - header data
//   - drag-n-drop
//   - fetch-on-demand
//   - submit/revert
//   - row insertion/removal/moving

class ConcatenationListModel:
    public ScopedModelOperations<AbstractMultiMappingModel<QAbstractListModel>>
{
    Q_OBJECT
    using base_type = ScopedModelOperations<AbstractMultiMappingModel<QAbstractListModel>>;

public:
    explicit ConcatenationListModel(QObject* parent = nullptr);

    explicit ConcatenationListModel(
        const std::initializer_list<QAbstractListModel*>& models,
        QObject* parent = nullptr);

    virtual ~ConcatenationListModel() override;

    QList<QAbstractListModel*> models() const;
    void setModels(const QList<QAbstractListModel*>& models);

    virtual QModelIndex mapToSource(const QModelIndex& index) const override;
    virtual QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    // All source models must support removeRows as unconditional removal of requested range,
    //    otherwise partial removal might happen.
    virtual bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;

private:
    void sourceModelAboutToBeReset();
    void sourceModelReset();
    void sourceRowsAboutToBeInserted(const QModelIndex& parent, int first, int last);
    void sourceRowsInserted();
    void sourceRowsAboutToBeRemoved(const QModelIndex& parent, int first, int last);
    void sourceRowsRemoved();
    void sourceRowsAboutToBeMoved(const QModelIndex& sourceParent, int sourceFirst, int sourceLast,
        const QModelIndex& destinationParent, int destinationRow);
    void sourceRowsMoved();
    void sourceLayoutAboutToBeChanged();
    void sourceLayoutChanged();

    int sourceModelPosition(const QAbstractItemModel* sourceModel) const;
    int mapFromSourceRow(const QAbstractItemModel* sourceModel, int row) const;

    void connectToModel(QAbstractListModel* model);
    int sourceRowCount(const QAbstractItemModel* sourceModel) const;

private:
    QList<QAbstractListModel*> m_models;
    nx::utils::ScopedConnections m_modelConnections;
    QVector<PersistentIndexPair> m_changingIndices;
    QAbstractListModel* m_ignoredModel = nullptr;
};

} // namespace nx::vms::client::desktop
