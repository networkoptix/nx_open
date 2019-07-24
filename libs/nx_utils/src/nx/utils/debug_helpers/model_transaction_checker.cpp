#include "model_transaction_checker.h"

#include <QtCore/QObject>
#include <QtCore/QAbstractItemModel>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log_message.h>

namespace nx::utils {

class ModelTransactionChecker::Private: public QObject
{
public:
    Private(QAbstractItemModel* model);

    enum class Operation
    {
        none = -1,
        rowInsert,
        rowRemove,
        rowMove,
        columnInsert,
        columnRemove,
        columnMove,
        layoutChange
    };

    friend QString toString(Operation value)
    {
        switch (value)
        {
            case Operation::none:
                return "none";
            case Operation::rowInsert:
                return "rowInsert";
            case Operation::rowRemove:
                return "rowRemove";
            case Operation::rowMove:
                return "rowMove";
            case Operation::columnInsert:
                return "columnInsert";
            case Operation::columnRemove:
                return "columnRemove";
            case Operation::columnMove:
                return "columnMove";
            case Operation::layoutChange:
                return "layoutChange";
            default:
                NX_ASSERT(false);
                return QString();
        }
    }

private:
    void handleRowsAboutToBeInserted(const QModelIndex& parent, int first, int last);
    void handleRowsInserted(const QModelIndex& parent, int first, int last);
    void handleRowsAboutToBeRemoved(const QModelIndex& parent, int first, int last);
    void handleRowsRemoved(const QModelIndex& parent, int first, int last);
    void handleRowsAboutToBeMoved(const QModelIndex& sourceParent, int sourceFirst, int sourceLast,
        const QModelIndex& destinationParent, int destinationPos);
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

    QAbstractItemModel* model()
    {
        return qobject_cast<QAbstractItemModel*>(sender());
    }

    static bool equals(const QModelIndex& left, const QModelIndex& right)
    {
        return (!left.isValid() && !right.isValid()) || (left == right);
    }

private:
    Operation m_currentOperation = Operation::none;
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
}

void ModelTransactionChecker::Private::handleRowsAboutToBeInserted(
    const QModelIndex& parent, int first, int last)
{
    NX_ASSERT(m_currentOperation == Operation::none, lm("rowsAboutToBeInserted while another "
        "operation is in progress: model=%1, operation=%2, parent=%3, first=%4, last=%5")
            .args(model(), m_currentOperation, parent, first, last));

    NX_ASSERT(model()->checkIndex(parent), lm("rowsAboutToBeInserted with invalid parent: "
        "model=%1, parent=%2, first=%3, last=%4").args(model(), parent, first, last));

    m_rowCount = model()->rowCount(parent);

    NX_ASSERT(last >= first && first >= 0 && first <= m_rowCount, lm("rowsAboutToBeInserted with "
        "invalid range: model=%1, parent=%2, first=%3, last=%4, rowCount=%5")
            .args(model(), parent, first, last, m_rowCount));

    m_currentOperation = Operation::rowInsert;
}

void ModelTransactionChecker::Private::handleRowsInserted(
    const QModelIndex& parent, int first, int last)
{
    NX_ASSERT(m_currentOperation == Operation::rowInsert, lm("rowsInserted while current "
        "operation is not row insert: model=%1, operation=%2, parent=%3, first=%4, last=%5")
            .args(model(), m_currentOperation, parent, first, last));

    const auto rowCount = model()->rowCount(parent);
    const auto expectedRowCount = m_rowCount + (last - first + 1);

    NX_ASSERT(rowCount == expectedRowCount, lm("rowsInserted resulted in wrong rowCount: "
        "model=%1, parent=%2, first=%3, last=%4, rowCount=%5, expected=%6")
            .args(model(), parent, first, last, rowCount, expectedRowCount));

    m_currentOperation = Operation::none;
}

void ModelTransactionChecker::Private::handleRowsAboutToBeRemoved(
    const QModelIndex& parent, int first, int last)
{
    NX_ASSERT(m_currentOperation == Operation::none, lm("rowsAboutToBeRemoved while another "
        "operation is in progress: model=%1, operation=%2, parent=%3, first=%4, last=%5")
            .args(model(), m_currentOperation, parent, first, last));

    NX_ASSERT(model()->checkIndex(parent), lm("rowsAboutToBeRemoved with invalid parent: "
        "model=%1, parent=%2, first=%3, last=%4").args(model(), parent, first, last));

    m_rowCount = model()->rowCount(parent);

    NX_ASSERT(last >= first && first >= 0 && last < m_rowCount, lm("rowsAboutToBeRemoved with "
        "invalid range: model=%1, parent=%2, first=%3, last=%4, rowCount=%5")
            .args(model(), parent, first, last, m_rowCount));

    m_currentOperation = Operation::rowRemove;
}

void ModelTransactionChecker::Private::handleRowsRemoved(
    const QModelIndex& parent, int first, int last)
{
    NX_ASSERT(m_currentOperation == Operation::rowRemove, lm("rowsRemoved while current "
        "operation is not row remove: model=%1, operation=%2, parent=%3, first=%4, last=%5")
            .args(model(), m_currentOperation, parent, first, last));

    const auto rowCount = model()->rowCount(parent);
    const auto expectedRowCount = m_rowCount - (last - first + 1);

    NX_ASSERT(rowCount == expectedRowCount, lm("rowsRemoved resulted in wrong rowCount: "
        "model=%1, parent=%2, first=%3, last=%4, rowCount=%5, expected=%6")
            .args(model(), parent, first, last, rowCount, expectedRowCount));

    m_currentOperation = Operation::none;
}

void ModelTransactionChecker::Private::handleRowsAboutToBeMoved(const QModelIndex& sourceParent,
    int sourceFirst, int sourceLast, const QModelIndex& destinationParent, int destinationPos)
{
    NX_ASSERT(m_currentOperation == Operation::none, lm("rowsAboutToBeMoved while another "
        "operation is in progress: model=%1, operation=%2, sourceParent=%3, sourceFirst=%4, "
            "sourceLast=%5, destinationParent=%6, destinationPos=%7")
                .args(model(), m_currentOperation, sourceParent, sourceFirst, sourceLast,
                    destinationParent, destinationPos));

    NX_ASSERT(model()->checkIndex(sourceParent), lm("rowsAboutToBeMoved with invalid sourceParent: "
        "model=%1, sourceParent=%2, sourceFirst=%3, sourceLast=%4, destinationParent=%5, "
            "destinationPos=%6").args(model(), sourceParent, sourceFirst, sourceLast,
                destinationParent, destinationPos));

    m_rowCount = model()->rowCount(sourceParent);
    const bool sameParent = equals(sourceParent, destinationParent);

    NX_ASSERT(sourceLast >= sourceFirst && sourceFirst >= 0 && sourceLast < m_rowCount,
        lm("rowsAboutToBeMoved with invalid source range: model=%1, sourceParent=%2, "
            "sourceFirst=%3, sourceLast=%4, destinationParent=%5, destinationPos=%6, "
                "sourceRowCount=%7").args(model(), sourceParent, sourceFirst, sourceLast,
                    destinationParent, destinationPos, m_rowCount));

    if (!sameParent)
    {
        NX_ASSERT(model()->checkIndex(destinationParent), lm("rowsAboutToBeMoved with invalid "
            "destinationParent: model=%1, sourceParent=%2, sourceFirst=%3, sourceLast=%4, "
                "destinationParent=%5, destinationPos=%6").args(model(), sourceParent, sourceFirst,
                    sourceLast, destinationParent, destinationPos));
    }

    m_destinationRowCount = sameParent ? m_rowCount : model()->rowCount(destinationParent);

    NX_ASSERT(destinationPos >= 0 && destinationPos <= m_destinationRowCount
            && (!sameParent || destinationPos < sourceFirst || destinationPos > sourceLast),
        lm("rowsAboutToBeMoved with invalid destination pos: model=%1, sourceParent=%2, "
            "sourceFirst=%3, sourceLast=%4, destinationParent=%5, destinationPos=%6"
                "destinationRowCount=%7").args(model(), sourceParent, sourceFirst, sourceLast,
                    destinationParent, destinationPos, m_destinationRowCount));

    m_currentOperation = Operation::rowMove;
}

void ModelTransactionChecker::Private::handleRowsMoved(const QModelIndex& sourceParent,
    int sourceFirst, int sourceLast, const QModelIndex& destinationParent, int destinationPos)
{
    NX_ASSERT(m_currentOperation == Operation::rowMove, lm("rowsMoved while current operation is "
        "not row move: model=%1, operation=%2, sourceParent=%3, sourceFirst=%4, sourceLast=%5, "
            "destinationParent=%6, destinationPos=%7").args(model(), m_currentOperation,
                sourceParent, sourceFirst, sourceLast, destinationParent, destinationPos));

    const auto sourceRowCount = model()->rowCount(sourceParent);
    if (sourceParent == destinationParent)
    {
        NX_ASSERT(sourceRowCount == m_rowCount, lm("rowsMoved within the same parent resulted in "
            "wrong rowCount: model=%1, sourceParent=%2, sourceFirst=%3, sourceLast=%4, "
                "destinationParent=%5, destinationPos=%6, rowCount=%7, expected=%8")
                    .args(model(), sourceParent, sourceFirst, sourceLast, destinationParent,
                        destinationPos, sourceRowCount, m_rowCount));
    }
    else
    {
        const auto count = sourceLast - sourceFirst + 1;
        const auto destinationRowCount = model()->rowCount(destinationParent);
        const auto expectedSourceRowCount = m_rowCount - count;
        const auto expectedDestinationRowCount = m_destinationRowCount + count;

        NX_ASSERT(sourceRowCount == expectedSourceRowCount, lm("rowsMoved to different parent "
            "resulted in wrong sourceRowCount: model=%1, sourceParent=%2, sourceFirst=%3, "
                "sourceLast=%4, destinationParent=%5, destinationPos=%6, sourceRowCount=%7, "
                    "expected=%8").args(model(), sourceParent, sourceFirst, sourceLast,
                        destinationParent, destinationPos, sourceRowCount, expectedSourceRowCount));

        NX_ASSERT(destinationRowCount == expectedDestinationRowCount, lm("rowsMoved to different "
            "parent resulted in wrong destinationRowCount: model=%1, sourceParent=%2, "
                "sourceFirst=%3, sourceLast=%4, destinationParent=%5, destinationPos=%6, "
                    "destinationRowCount=%7, expected=%8").args(model(), sourceParent, sourceFirst,
                        sourceLast, destinationParent, destinationPos, destinationRowCount,
                            expectedDestinationRowCount));
    }

    m_currentOperation = Operation::none;
}

void ModelTransactionChecker::Private::handleColumnsAboutToBeInserted(
    const QModelIndex& parent, int first, int last)
{
    NX_ASSERT(m_currentOperation == Operation::none, lm("columnsAboutToBeInserted while another "
        "operation is in progress: model=%1, operation=%2, parent=%3, first=%4, last=%5")
            .args(model(), m_currentOperation, parent, first, last));

    NX_ASSERT(model()->checkIndex(parent), lm("columnsAboutToBeInserted with invalid parent: "
        "model=%1, parent=%2, first=%3, last=%4").args(model(), parent, first, last));

    m_columnCount = model()->columnCount(parent);

    NX_ASSERT(last >= first && first >= 0 && first <= m_columnCount, lm("columnsAboutToBeInserted "
        "with invalid range: model=%1, parent=%2, first=%3, last=%4, columnCount=%5")
            .args(model(), parent, first, last, m_columnCount));

    m_currentOperation = Operation::columnInsert;
}

void ModelTransactionChecker::Private::handleColumnsInserted(
    const QModelIndex& parent, int first, int last)
{
    NX_ASSERT(m_currentOperation == Operation::columnInsert, lm("columnsInserted while current "
        "operation is not column insert: model=%1, operation=%2, parent=%3, first=%4, last=%5")
            .args(model(), m_currentOperation, parent, first, last));

    const auto columnCount = model()->columnCount(parent);
    const auto expectedColumnCount = m_columnCount + (last - first + 1);

    NX_ASSERT(columnCount == expectedColumnCount, lm("columnsInserted resulted in wrong "
        "columnCount: model=%1, parent=%2, first=%3, last=%4, columnCount=%5, expected=%6")
            .args(model(), parent, first, last, columnCount, expectedColumnCount));

    m_currentOperation = Operation::none;
}

void ModelTransactionChecker::Private::handleColumnsAboutToBeRemoved(
    const QModelIndex& parent, int first, int last)
{
    NX_ASSERT(m_currentOperation == Operation::none, lm("columnsAboutToBeRemoved while another "
        "operation is in progress: model=%1, operation=%2, parent=%3, first=%4, last=%5")
            .args(model(), m_currentOperation, parent, first, last));

    NX_ASSERT(model()->checkIndex(parent), lm("columnsAboutToBeRemoved with invalid parent: "
        "model=%1, parent=%2, first=%3, last=%4").args(model(), parent, first, last));

    m_columnCount = model()->columnCount(parent);

    NX_ASSERT(last >= first && first >= 0 && last < m_columnCount, lm("columnsAboutToBeRemoved with "
        "invalid range: model=%1, parent=%2, first=%3, last=%4, columnCount=%5")
            .args(model(), parent, first, last, m_columnCount));

    m_currentOperation = Operation::columnRemove;
}

void ModelTransactionChecker::Private::handleColumnsRemoved(
    const QModelIndex& parent, int first, int last)
{
    NX_ASSERT(m_currentOperation == Operation::columnRemove, lm("columnsRemoved while current "
        "operation is not column remove: model=%1, operation=%2, parent=%3, first=%4, last=%5")
            .args(model(), m_currentOperation, parent, first, last));

    const auto columnCount = model()->columnCount(parent);
    const auto expectedColumnCount = m_columnCount - (last - first + 1);

    NX_ASSERT(columnCount == expectedColumnCount, lm("columnsRemoved resulted in wrong "
        "columnCount: model=%1, parent=%2, first=%3, last=%4, columnCount=%5, expected=%6")
            .args(model(), parent, first, last, columnCount, expectedColumnCount));

    m_currentOperation = Operation::none;
}

void ModelTransactionChecker::Private::handleColumnsAboutToBeMoved(const QModelIndex& sourceParent,
    int sourceFirst, int sourceLast, const QModelIndex& destinationParent, int destinationPos)
{
    NX_ASSERT(m_currentOperation == Operation::none, lm("columnsAboutToBeMoved while another "
        "operation is in progress: model=%1, operation=%2, sourceParent=%3, sourceFirst=%4, "
            "sourceLast=%5, destinationParent=%6, destinationPos=%7")
                .args(model(), m_currentOperation, sourceParent, sourceFirst, sourceLast,
                    destinationParent, destinationPos));

    NX_ASSERT(model()->checkIndex(sourceParent), lm("columnsAboutToBeMoved with invalid "
        "sourceParent: model=%1, sourceParent=%2, sourceFirst=%3, sourceLast=%4, "
            "destinationParent=%5, destinationPos=%6").args(model(), sourceParent, sourceFirst,
                sourceLast, destinationParent, destinationPos));

    m_columnCount = model()->columnCount(sourceParent);
    const bool sameParent = equals(sourceParent, destinationParent);

    NX_ASSERT(sourceLast >= sourceFirst && sourceFirst >= 0 && sourceLast < m_columnCount,
        lm("columnsAboutToBeMoved with invalid source range: model=%1, sourceParent=%2, "
            "sourceFirst=%3, sourceLast=%4, destinationParent=%5, destinationPos=%6, "
                "sourceColumnCount=%7").args(model(), sourceParent, sourceFirst, sourceLast,
                    destinationParent, destinationPos, m_columnCount));

    if (!sameParent)
    {
        NX_ASSERT(model()->checkIndex(destinationParent), lm("columnsAboutToBeMoved with invalid "
            "destinationParent: model=%1, sourceParent=%2, sourceFirst=%3, sourceLast=%4, "
                "destinationParent=%5, destinationPos=%6").args(model(), sourceParent, sourceFirst,
                    sourceLast, destinationParent, destinationPos));
    }

    m_destinationColumnCount = sameParent ? m_columnCount : model()->columnCount(destinationParent);

    NX_ASSERT(destinationPos >= 0 && destinationPos <= m_destinationColumnCount
            && (!sameParent || destinationPos < sourceFirst || destinationPos > sourceLast),
        lm("columnsAboutToBeMoved with invalid destination pos: model=%1, sourceParent=%2, "
            "sourceFirst=%3, sourceLast=%4, destinationParent=%5, destinationPos=%6"
                "destinationColumnCount=%7").args(model(), sourceParent, sourceFirst, sourceLast,
                    destinationParent, destinationPos, m_destinationColumnCount));

    m_currentOperation = Operation::columnMove;
}

void ModelTransactionChecker::Private::handleColumnsMoved(const QModelIndex& sourceParent,
    int sourceFirst, int sourceLast, const QModelIndex& destinationParent, int destinationPos)
{
    NX_ASSERT(m_currentOperation == Operation::columnMove, lm("columnsMoved while current "
        "operation is not column move: model=%1, operation=%2, sourceParent=%3, sourceFirst=%4, "
            "sourceLast=%5, destinationParent=%6, destinationPos=%7")
                .args(model(), m_currentOperation, sourceParent, sourceFirst, sourceLast,
                    destinationParent, destinationPos));

    const auto sourceColumnCount = model()->columnCount(sourceParent);
    if (sourceParent == destinationParent)
    {
        NX_ASSERT(sourceColumnCount == m_columnCount, lm("columnsMoved within the same parent "
            "resulted in wrong columnCount: model=%1, sourceParent=%2, sourceFirst=%3, "
                "sourceLast=%4, destinationParent=%5, destinationPos=%6, columnCount=%7, "
                    "expected=%8").args(model(), sourceParent, sourceFirst, sourceLast,
                        destinationParent, destinationPos, sourceColumnCount, m_columnCount));
    }
    else
    {
        const auto count = sourceLast - sourceFirst + 1;
        const auto destinationColumnCount = model()->columnCount(destinationParent);
        const auto expectedSourceColumnCount = m_columnCount - count;
        const auto expectedDestinationColumnCount = m_destinationColumnCount + count;

        NX_ASSERT(sourceColumnCount == expectedSourceColumnCount, lm("columnsMoved to different "
            "parent resulted in wrong sourceColumnCount: model=%1, sourceParent=%2, "
                "sourceFirst=%3, sourceLast=%4, destinationParent=%5, destinationPos=%6, "
                    "sourceColumnCount=%7, expected=%8").args(model(), sourceParent, sourceFirst,
                        sourceLast, destinationParent, destinationPos, sourceColumnCount,
                            expectedSourceColumnCount));

        NX_ASSERT(destinationColumnCount == expectedDestinationColumnCount, lm("columnsMoved to "
            "different parent resulted in wrong destinationColumnCount: model=%1, sourceParent=%2, "
                "sourceFirst=%3, sourceLast=%4, destinationParent=%5, destinationPos=%6, "
                    "destinationColumnCount=%7, expected=%8").args(model(), sourceParent,
                        sourceFirst, sourceLast, destinationParent, destinationPos,
                            destinationColumnCount,expectedDestinationColumnCount));
    }

    m_currentOperation = Operation::none;
}

void ModelTransactionChecker::Private::handleDataChanged(
    const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    NX_ASSERT(m_currentOperation == Operation::none, lm("dataChanged while structure change is "
        "in progress: model=%1, operation=%2, topLeft=%3, bottomRight=%4")
            .args(model(), m_currentOperation, topLeft, bottomRight));

    NX_ASSERT(model()->checkIndex(topLeft) && model()->checkIndex(bottomRight),
        lm("dataChanged with invalid index range: model=%1, topLeft=%3, bottomRight=%4")
            .args(model(), topLeft, bottomRight));

    NX_ASSERT(equals(topLeft.parent(), bottomRight.parent()),
        lm("dataChanged with topLeft and bottomRight in different parents: model=%1, "
            "topLeft=%3, bottomRight=%4, topLeftParent=%5, bottomRightParent=%6")
                .args(model(), topLeft, bottomRight, topLeft.parent(), bottomRight.parent()));
}

void ModelTransactionChecker::Private::handleLayoutAboutToBeChanged(
    const QList<QPersistentModelIndex>& parents)
{
    NX_ASSERT(m_currentOperation == Operation::none,
        lm("layoutAboutToBeChanged while another operation is in progress: model=%1, operation=%2")
            .args(model(), m_currentOperation));

    m_currentOperation = Operation::layoutChange;
}

void ModelTransactionChecker::Private::handleLayoutChanged(
    const QList<QPersistentModelIndex>& parents)
{
    NX_ASSERT(m_currentOperation == Operation::layoutChange,
        lm("layoutChanged while current operation is not layout change: model=%1, operation=%2")
            .args(model(), m_currentOperation));

    m_currentOperation = Operation::none;
}

} // namespace nx::utils
