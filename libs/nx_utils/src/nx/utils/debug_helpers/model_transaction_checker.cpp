// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "model_transaction_checker.h"

#include <QtCore/QObject>
#include <QtCore/QAbstractItemModel>

#include <nx/utils/debug_helpers/model_operation.h>
#include <nx/utils/log/assert.h>

namespace nx::utils {

class ModelTransactionChecker::Private: public QObject
{
public:
    Private(QAbstractItemModel* model);

private:
    void handleRowsAboutToBeInserted(const QModelIndex& parent, int first, int last);
    void handleRowsInserted(const QModelIndex& parent, int first, int last);
    void handleRowsAboutToBeRemoved(const QModelIndex& parent, int first, int last);
    void handleRowsRemoved(const QModelIndex& parent, int first, int last);
    void handleRowsAboutToBeMoved(const QModelIndex& sourceParent, int sourceFirst,
        int sourceLast, const QModelIndex& destinationParent, int destinationPos);
    void handleRowsMoved(const QModelIndex& sourceParent, int sourceFirst, int sourceLast,
        const QModelIndex& destinationParent, int destinationPos);
    void handleColumnsAboutToBeInserted(const QModelIndex& parent, int first, int last);
    void handleColumnsInserted(const QModelIndex& parent, int first, int last);
    void handleColumnsAboutToBeRemoved(const QModelIndex& parent, int first, int last);
    void handleColumnsRemoved(const QModelIndex& parent, int first, int last);
    void handleColumnsAboutToBeMoved(const QModelIndex& sourceParent, int sourceFirst,
        int sourceLast, const QModelIndex& destinationParent, int destinationPos);
    void handleColumnsMoved(const QModelIndex& sourceParent, int sourceFirst, int sourceLast,
        const QModelIndex& destinationParent, int destinationPos);
    void handleDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);
    void handleLayoutAboutToBeChanged(const QList<QPersistentModelIndex>& parents);
    void handleLayoutChanged(const QList<QPersistentModelIndex>& parents);
    void handleModelAboutToBeReset();
    void handleModelReset();

    QAbstractItemModel* model()
    {
        return qobject_cast<QAbstractItemModel*>(sender());
    }

    static bool equals(const QModelIndex& left, const QModelIndex& right)
    {
        return (!left.isValid() && !right.isValid()) || (left == right);
    }

private:
    ModelOperation m_currentOperation;
    int m_rowCount = 0;
    int m_columnCount = 0;
    int m_destinationRowCount = 0;
    int m_destinationColumnCount = 0;
};

ModelTransactionChecker* ModelTransactionChecker::install(QAbstractItemModel* model)
{
    return NX_ASSERT(model)
        ? new ModelTransactionChecker(model)
        : nullptr;
}

ModelTransactionChecker::ModelTransactionChecker(QAbstractItemModel* parent):
    QObject(parent),
    d(new Private(parent))
{
}

ModelTransactionChecker::~ModelTransactionChecker()
{
}

ModelTransactionChecker::Private::Private(QAbstractItemModel* model)
{
    if (!NX_ASSERT(model))
        return;

    connect(model, &QAbstractItemModel::rowsAboutToBeInserted,
        this, &Private::handleRowsAboutToBeInserted, Qt::DirectConnection);

    connect(model, &QAbstractItemModel::rowsInserted,
        this, &Private::handleRowsInserted, Qt::DirectConnection);

    connect(model, &QAbstractItemModel::rowsAboutToBeRemoved,
        this, &Private::handleRowsAboutToBeRemoved, Qt::DirectConnection);

    connect(model, &QAbstractItemModel::rowsRemoved,
        this, &Private::handleRowsRemoved, Qt::DirectConnection);

    connect(model, &QAbstractItemModel::rowsAboutToBeMoved,
        this, &Private::handleRowsAboutToBeMoved, Qt::DirectConnection);

    connect(model, &QAbstractItemModel::rowsMoved,
        this, &Private::handleRowsMoved, Qt::DirectConnection);

    connect(model, &QAbstractItemModel::columnsAboutToBeInserted,
        this, &Private::handleColumnsAboutToBeInserted, Qt::DirectConnection);

    connect(model, &QAbstractItemModel::columnsInserted,
        this, &Private::handleColumnsInserted, Qt::DirectConnection);

    connect(model, &QAbstractItemModel::columnsAboutToBeRemoved,
        this, &Private::handleColumnsAboutToBeRemoved, Qt::DirectConnection);

    connect(model, &QAbstractItemModel::columnsRemoved,
        this, &Private::handleColumnsRemoved, Qt::DirectConnection);

    connect(model, &QAbstractItemModel::columnsAboutToBeMoved,
        this, &Private::handleColumnsAboutToBeMoved, Qt::DirectConnection);

    connect(model, &QAbstractItemModel::columnsMoved,
        this, &Private::handleColumnsMoved, Qt::DirectConnection);

    connect(model, &QAbstractItemModel::dataChanged,
        this, &Private::handleDataChanged, Qt::DirectConnection);

    connect(model, &QAbstractItemModel::layoutAboutToBeChanged,
        this, &Private::handleLayoutAboutToBeChanged, Qt::DirectConnection);

    connect(model, &QAbstractItemModel::layoutChanged,
        this, &Private::handleLayoutChanged, Qt::DirectConnection);

    connect(model, &QAbstractItemModel::modelAboutToBeReset,
        this, &Private::handleModelAboutToBeReset, Qt::DirectConnection);

    connect(model, &QAbstractItemModel::modelReset,
        this, &Private::handleModelReset, Qt::DirectConnection);
}

void ModelTransactionChecker::Private::handleRowsAboutToBeInserted(
    const QModelIndex& parent, int first, int last)
{
    NX_ASSERT(m_currentOperation.begin(ModelOperation::rowInsert),
        "rowsAboutToBeInserted while another row changing operation is in progress: "
            "model=%1, operation=%2, parent=%3, first=%4, last=%5",
                model(), m_currentOperation, parent, first, last);

    NX_ASSERT(model()->checkIndex(parent), "rowsAboutToBeInserted with invalid parent: "
        "model=%1, parent=%2, first=%3, last=%4", model(), parent, first, last);

    m_rowCount = model()->rowCount(parent);

    NX_ASSERT(last >= first && first >= 0 && first <= m_rowCount,
        "rowsAboutToBeInserted with invalid range: model=%1, parent=%2, first=%3, "
            "last=%4, rowCount=%5", model(), parent, first, last, m_rowCount);
}

void ModelTransactionChecker::Private::handleRowsInserted(
    const QModelIndex& parent, int first, int last)
{
    NX_ASSERT(m_currentOperation.end(ModelOperation::rowInsert),
        "rowsInserted while no row insertion operation is in progress: "
            "model=%1, operation=%2, parent=%3, first=%4, last=%5",
                model(), m_currentOperation, parent, first, last);

    const auto rowCount = model()->rowCount(parent);
    const auto expectedRowCount = m_rowCount + (last - first + 1);

    NX_ASSERT(rowCount == expectedRowCount, "rowsInserted resulted in wrong rowCount: "
        "model=%1, parent=%2, first=%3, last=%4, rowCount=%5, expected=%6",
            model(), parent, first, last, rowCount, expectedRowCount);
}

void ModelTransactionChecker::Private::handleRowsAboutToBeRemoved(
    const QModelIndex& parent, int first, int last)
{
    NX_ASSERT(m_currentOperation.begin(ModelOperation::rowRemove),
        "rowsAboutToBeRemoved while another row changing operation is in progress: "
            "model=%1, operation=%2, parent=%3, first=%4, last=%5",
                model(), m_currentOperation, parent, first, last);

    NX_ASSERT(model()->checkIndex(parent), "rowsAboutToBeRemoved with invalid parent: "
        "model=%1, parent=%2, first=%3, last=%4", model(), parent, first, last);

    m_rowCount = model()->rowCount(parent);

    NX_ASSERT(last >= first && first >= 0 && last < m_rowCount,
        "rowsAboutToBeRemoved with invalid range: model=%1, parent=%2, first=%3, "
            "last=%4, rowCount=%5", model(), parent, first, last, m_rowCount);
}

void ModelTransactionChecker::Private::handleRowsRemoved(
    const QModelIndex& parent, int first, int last)
{
    NX_ASSERT(m_currentOperation.end(ModelOperation::rowRemove),
        "rowsRemoved while no row removal operation is in progress: "
            "model=%1, operation=%2, parent=%3, first=%4, last=%5",
                model(), m_currentOperation, parent, first, last);

    const auto rowCount = model()->rowCount(parent);
    const auto expectedRowCount = m_rowCount - (last - first + 1);

    NX_ASSERT(rowCount == expectedRowCount, "rowsRemoved resulted in wrong rowCount: "
        "model=%1, parent=%2, first=%3, last=%4, rowCount=%5, expected=%6",
            model(), parent, first, last, rowCount, expectedRowCount);
}

void ModelTransactionChecker::Private::handleRowsAboutToBeMoved(const QModelIndex& sourceParent,
    int sourceFirst, int sourceLast, const QModelIndex& destinationParent, int destinationPos)
{
    NX_ASSERT(m_currentOperation.begin(ModelOperation::rowMove),
        "rowsAboutToBeMoved while another row changing operation is in progress: "
            "model=%1, operation=%2, sourceParent=%3, sourceFirst=%4, sourceLast=%5, "
                "destinationParent=%6, destinationPos=%7", model(), m_currentOperation,
                    sourceParent, sourceFirst, sourceLast, destinationParent, destinationPos);

    NX_ASSERT(model()->checkIndex(sourceParent), "rowsAboutToBeMoved with invalid sourceParent: "
        "model=%1, sourceParent=%2, sourceFirst=%3, sourceLast=%4, destinationParent=%5, "
            "destinationPos=%6", model(), sourceParent, sourceFirst, sourceLast,
                destinationParent, destinationPos);

    m_rowCount = model()->rowCount(sourceParent);
    const bool sameParent = equals(sourceParent, destinationParent);

    NX_ASSERT(sourceLast >= sourceFirst && sourceFirst >= 0 && sourceLast < m_rowCount,
        "rowsAboutToBeMoved with invalid source range: model=%1, sourceParent=%2, "
            "sourceFirst=%3, sourceLast=%4, destinationParent=%5, destinationPos=%6, "
                "sourceRowCount=%7", model(), sourceParent, sourceFirst, sourceLast,
                    destinationParent, destinationPos, m_rowCount);

    if (!sameParent)
    {
        NX_ASSERT(model()->checkIndex(destinationParent),
            "rowsAboutToBeMoved with invalid destinationParent: "
                "model=%1, sourceParent=%2, sourceFirst=%3, sourceLast=%4, destinationParent=%5, "
                    "destinationPos=%6", model(), sourceParent, sourceFirst, sourceLast,
                        destinationParent, destinationPos);
    }

    m_destinationRowCount = sameParent ? m_rowCount : model()->rowCount(destinationParent);

    NX_ASSERT(destinationPos >= 0 && destinationPos <= m_destinationRowCount
            && (!sameParent || destinationPos < sourceFirst || destinationPos > sourceLast),
        "rowsAboutToBeMoved with invalid destination pos: model=%1, sourceParent=%2, "
            "sourceFirst=%3, sourceLast=%4, destinationParent=%5, destinationPos=%6"
                "destinationRowCount=%7", model(), sourceParent, sourceFirst, sourceLast,
                    destinationParent, destinationPos, m_destinationRowCount);
}

void ModelTransactionChecker::Private::handleRowsMoved(const QModelIndex& sourceParent,
    int sourceFirst, int sourceLast, const QModelIndex& destinationParent, int destinationPos)
{
    NX_ASSERT(m_currentOperation.end(ModelOperation::rowMove),
        "rowsMoved while no row move operation is in progress:"
            "model=%1, operation=%2, sourceParent=%3, sourceFirst=%4, sourceLast=%5, "
                "destinationParent=%6, destinationPos=%7", model(), m_currentOperation,
                    sourceParent, sourceFirst, sourceLast, destinationParent, destinationPos);

    const auto sourceRowCount = model()->rowCount(sourceParent);
    if (sourceParent == destinationParent)
    {
        NX_ASSERT(sourceRowCount == m_rowCount,
            "rowsMoved within the same parent resulted in wrong rowCount: "
                "model=%1, sourceParent=%2, sourceFirst=%3, sourceLast=%4, "
                    "destinationParent=%5, destinationPos=%6, rowCount=%7, expected=%8",
                        model(), sourceParent, sourceFirst, sourceLast, destinationParent,
                            destinationPos, sourceRowCount, m_rowCount);
    }
    else
    {
        const auto count = sourceLast - sourceFirst + 1;
        const auto destinationRowCount = model()->rowCount(destinationParent);
        const auto expectedSourceRowCount = m_rowCount - count;
        const auto expectedDestinationRowCount = m_destinationRowCount + count;

        NX_ASSERT(sourceRowCount == expectedSourceRowCount,
            "rowsMoved to different parent resulted in wrong sourceRowCount: "
                "model=%1, sourceParent=%2, sourceFirst=%3, sourceLast=%4, "
                    "destinationParent=%5, destinationPos=%6, sourceRowCount=%7, expected=%8",
                        model(), sourceParent, sourceFirst, sourceLast, destinationParent,
                            destinationPos, sourceRowCount, expectedSourceRowCount);

        NX_ASSERT(destinationRowCount == expectedDestinationRowCount,
            "rowsMoved to different parent resulted in wrong destinationRowCount: "
                "model=%1, sourceParent=%2, sourceFirst=%3, sourceLast=%4, destinationParent=%5, "
                    "destinationPos=%6, destinationRowCount=%7, expected=%8",
                        model(), sourceParent, sourceFirst, sourceLast, destinationParent,
                            destinationPos, destinationRowCount, expectedDestinationRowCount);
    }
}

void ModelTransactionChecker::Private::handleColumnsAboutToBeInserted(
    const QModelIndex& parent, int first, int last)
{
    NX_ASSERT(m_currentOperation.begin(ModelOperation::columnInsert),
        "columnsAboutToBeInserted while another column change operation is in progress: "
            "model=%1, operation=%2, parent=%3, first=%4, last=%5",
                model(), m_currentOperation, parent, first, last);

    NX_ASSERT(model()->checkIndex(parent), "columnsAboutToBeInserted with invalid parent: "
        "model=%1, parent=%2, first=%3, last=%4", model(), parent, first, last);

    m_columnCount = model()->columnCount(parent);

    NX_ASSERT(last >= first && first >= 0 && first <= m_columnCount,
        "columnsAboutToBeInserted with invalid range: "
            "model=%1, parent=%2, first=%3, last=%4, columnCount=%5",
                model(), parent, first, last, m_columnCount);
}

void ModelTransactionChecker::Private::handleColumnsInserted(
    const QModelIndex& parent, int first, int last)
{
    NX_ASSERT(m_currentOperation.end(ModelOperation::columnInsert),
        "columnsInserted while no column insertion operation is in progress: "
            "model=%1, operation=%2, parent=%3, first=%4, last=%5",
                model(), m_currentOperation, parent, first, last);

    const auto columnCount = model()->columnCount(parent);
    const auto expectedColumnCount = m_columnCount + (last - first + 1);

    NX_ASSERT(columnCount == expectedColumnCount, "columnsInserted resulted in wrong "
        "columnCount: model=%1, parent=%2, first=%3, last=%4, columnCount=%5, expected=%6",
            model(), parent, first, last, columnCount, expectedColumnCount);
}

void ModelTransactionChecker::Private::handleColumnsAboutToBeRemoved(
    const QModelIndex& parent, int first, int last)
{
    NX_ASSERT(m_currentOperation.begin(ModelOperation::columnRemove),
        "columnsAboutToBeRemoved while another column change operation is in progress: "
            "model=%1, operation=%2, parent=%3, first=%4, last=%5",
                model(), m_currentOperation, parent, first, last);

    NX_ASSERT(model()->checkIndex(parent),
        "columnsAboutToBeRemoved with invalid parent: "
            "model=%1, parent=%2, first=%3, last=%4", model(), parent, first, last);

    m_columnCount = model()->columnCount(parent);

    NX_ASSERT(last >= first && first >= 0 && last < m_columnCount,
        "columnsAboutToBeRemoved with invalid range: "
            "model=%1, parent=%2, first=%3, last=%4, columnCount=%5",
                model(), parent, first, last, m_columnCount);
}

void ModelTransactionChecker::Private::handleColumnsRemoved(
    const QModelIndex& parent, int first, int last)
{
    NX_ASSERT(m_currentOperation.end(ModelOperation::columnRemove),
        "columnsRemoved while no column remove operation is in progress: "
            "model=%1, operation=%2, parent=%3, first=%4, last=%5",
                model(), m_currentOperation, parent, first, last);

    const auto columnCount = model()->columnCount(parent);
    const auto expectedColumnCount = m_columnCount - (last - first + 1);

    NX_ASSERT(columnCount == expectedColumnCount, "columnsRemoved resulted in wrong "
        "columnCount: model=%1, parent=%2, first=%3, last=%4, columnCount=%5, expected=%6",
            model(), parent, first, last, columnCount, expectedColumnCount);
}

void ModelTransactionChecker::Private::handleColumnsAboutToBeMoved(
    const QModelIndex& sourceParent, int sourceFirst, int sourceLast,
    const QModelIndex& destinationParent, int destinationPos)
{
    NX_ASSERT(m_currentOperation.begin(ModelOperation::columnMove),
        "columnsAboutToBeRemoved while another column change operation is in progress: "
            "model=%1, operation=%2, sourceParent=%3, sourceFirst=%4, sourceLast=%5, "
                "destinationParent=%6, destinationPos=%7",
                    model(), m_currentOperation, sourceParent, sourceFirst, sourceLast,
                        destinationParent, destinationPos);

    NX_ASSERT(model()->checkIndex(sourceParent), "columnsAboutToBeMoved with invalid "
        "sourceParent: model=%1, sourceParent=%2, sourceFirst=%3, sourceLast=%4, "
            "destinationParent=%5, destinationPos=%6", model(), sourceParent, sourceFirst,
                sourceLast, destinationParent, destinationPos);

    m_columnCount = model()->columnCount(sourceParent);
    const bool sameParent = equals(sourceParent, destinationParent);

    NX_ASSERT(sourceLast >= sourceFirst && sourceFirst >= 0 && sourceLast < m_columnCount,
        "columnsAboutToBeMoved with invalid source range: model=%1, sourceParent=%2, "
            "sourceFirst=%3, sourceLast=%4, destinationParent=%5, destinationPos=%6, "
                "sourceColumnCount=%7", model(), sourceParent, sourceFirst, sourceLast,
                    destinationParent, destinationPos, m_columnCount);

    if (!sameParent)
    {
        NX_ASSERT(model()->checkIndex(destinationParent), "columnsAboutToBeMoved with invalid "
            "destinationParent: model=%1, sourceParent=%2, sourceFirst=%3, sourceLast=%4, "
                "destinationParent=%5, destinationPos=%6", model(), sourceParent, sourceFirst,
                    sourceLast, destinationParent, destinationPos);
    }

    m_destinationColumnCount = sameParent
        ? m_columnCount
        : model()->columnCount(destinationParent);

    NX_ASSERT(destinationPos >= 0 && destinationPos <= m_destinationColumnCount
            && (!sameParent || destinationPos < sourceFirst || destinationPos > sourceLast),
        "columnsAboutToBeMoved with invalid destination pos: model=%1, sourceParent=%2, "
            "sourceFirst=%3, sourceLast=%4, destinationParent=%5, destinationPos=%6"
                "destinationColumnCount=%7", model(), sourceParent, sourceFirst, sourceLast,
                    destinationParent, destinationPos, m_destinationColumnCount);
}

void ModelTransactionChecker::Private::handleColumnsMoved(const QModelIndex& sourceParent,
    int sourceFirst, int sourceLast, const QModelIndex& destinationParent, int destinationPos)
{
    NX_ASSERT(m_currentOperation.end(ModelOperation::columnMove), "columnsMoved while current "
        "operation is not column move: model=%1, operation=%2, sourceParent=%3, sourceFirst=%4, "
            "sourceLast=%5, destinationParent=%6, destinationPos=%7",
                model(), m_currentOperation, sourceParent, sourceFirst, sourceLast,
                    destinationParent, destinationPos);

    const auto sourceColumnCount = model()->columnCount(sourceParent);
    if (sourceParent == destinationParent)
    {
        NX_ASSERT(sourceColumnCount == m_columnCount, "columnsMoved within the same "
            "parent resulted in wrong columnCount: model=%1, sourceParent=%2, sourceFirst=%3, "
                "sourceLast=%4, destinationParent=%5, destinationPos=%6, columnCount=%7, "
                    "expected=%8", model(), sourceParent, sourceFirst, sourceLast,
                        destinationParent, destinationPos, sourceColumnCount, m_columnCount);
    }
    else
    {
        const auto count = sourceLast - sourceFirst + 1;
        const auto destinationColumnCount = model()->columnCount(destinationParent);
        const auto expectedSourceColumnCount = m_columnCount - count;
        const auto expectedDestinationColumnCount = m_destinationColumnCount + count;

        NX_ASSERT(sourceColumnCount == expectedSourceColumnCount, "columnsMoved to "
            "different parent resulted in wrong sourceColumnCount: model=%1, sourceParent=%2, "
                "sourceFirst=%3, sourceLast=%4, destinationParent=%5, destinationPos=%6, "
                    "sourceColumnCount=%7, expected=%8", model(), sourceParent, sourceFirst,
                        sourceLast, destinationParent, destinationPos, sourceColumnCount,
                            expectedSourceColumnCount);

        NX_ASSERT(destinationColumnCount == expectedDestinationColumnCount,
            "columnsMoved to different parent resulted in wrong destinationColumnCount: "
                "model=%1, sourceParent=%2, sourceFirst=%3, sourceLast=%4, destinationParent=%5, "
                    "destinationPos=%6, destinationColumnCount=%7, expected=%8", model(),
                        sourceParent, sourceFirst, sourceLast, destinationParent, destinationPos,
                            destinationColumnCount,expectedDestinationColumnCount);
    }
}

void ModelTransactionChecker::Private::handleDataChanged(
    const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    NX_ASSERT(m_currentOperation == ModelOperation::none, "dataChanged while structure change "
        "is in progress: model=%1, operation=%2, topLeft=%3, bottomRight=%4",
            model(), m_currentOperation, topLeft, bottomRight);

    NX_ASSERT(model()->checkIndex(topLeft) && model()->checkIndex(bottomRight),
        "dataChanged with invalid index range: model=%1, topLeft=%3, bottomRight=%4",
            model(), topLeft, bottomRight);

    NX_ASSERT(equals(topLeft.parent(), bottomRight.parent()),
        "dataChanged with topLeft and bottomRight in different parents: model=%1, "
            "topLeft=%3, bottomRight=%4, topLeftParent=%5, bottomRightParent=%6",
                model(), topLeft, bottomRight, topLeft.parent(), bottomRight.parent());
}

void ModelTransactionChecker::Private::handleLayoutAboutToBeChanged(
    const QList<QPersistentModelIndex>& /*parents*/)
{
    NX_ASSERT(m_currentOperation.begin(ModelOperation::layoutChange),
        "layoutAboutToBeChanged while another operation is in progress: model=%1, operation=%2",
            model(), m_currentOperation);
}

void ModelTransactionChecker::Private::handleLayoutChanged(
    const QList<QPersistentModelIndex>& /*parents*/)
{
    NX_ASSERT(m_currentOperation.end(ModelOperation::layoutChange),
        "layoutChanged while current operation is not layout change: model=%1, operation=%2",
            model(), m_currentOperation);
}

void ModelTransactionChecker::Private::handleModelAboutToBeReset()
{
    NX_ASSERT(m_currentOperation.begin(ModelOperation::modelReset),
        "modelAboutToBeReset while another operation is in progress: model=%1, operation=%2",
            model(), m_currentOperation);
}

void ModelTransactionChecker::Private::handleModelReset()
{
    NX_ASSERT(m_currentOperation.end(ModelOperation::modelReset),
        "modelReset while current operation is not a reset: model=%1, operation=%2",
            model(), m_currentOperation);
}

} // namespace nx::utils
