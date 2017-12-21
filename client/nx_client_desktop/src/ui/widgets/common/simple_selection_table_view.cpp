#include "simple_selection_table_view.h"

#include <QtGui/QKeyEvent>

#include <ui/widgets/views/checkboxed_header_view.h>

QnSimpleSelectionTableView::QnSimpleSelectionTableView(QWidget* parent):
    base_type(parent)
{
    connect(this, &QnSimpleSelectionTableView::clicked,
        this, &QnSimpleSelectionTableView::handleClicked);
}

QnSimpleSelectionTableView::~QnSimpleSelectionTableView()
{
}


void QnSimpleSelectionTableView::setCheckboxColumn(int column, bool checkboxInHeader)
{
    m_checkboxColumn = column;

    if (!checkboxInHeader)
    {
        m_checkableHeader = nullptr;
        setHorizontalHeader(new QHeaderView(Qt::Horizontal, this));
    }
    else
    {
        m_checkableHeader = new QnCheckBoxedHeaderView(column, this);
        setHorizontalHeader(m_checkableHeader);
        setupHeader();
    }
}

int QnSimpleSelectionTableView::getCheckedCount() const
{
    const auto currentModel = model();
    if (!currentModel)
        return 0;

    int result = 0;
    const int rowCount = currentModel->rowCount();
    for (int row = 0; row != rowCount; ++row)
    {
        const auto index = currentModel->index(row, m_checkboxColumn);
        const bool checkedState = index.data(Qt::CheckStateRole).value<Qt::CheckState>();
        if (checkedState != Qt::Unchecked)
            ++result;
    }
    return result;
}

void QnSimpleSelectionTableView::setupHeader()
{
    const auto currentModel = model();
    if (!currentModel || !m_checkableHeader)
        return;

    connect(m_checkableHeader, &QnCheckBoxedHeaderView::checkStateChanged,
        this, &QnSimpleSelectionTableView::handleHeaderCheckedStateChanged);

    const auto dataChangedHandler =
        [this, currentModel](
            const QModelIndex& /*topLeft*/,
            const QModelIndex& /*bottomRight*/,
            QVector<int> roles)
        {
            const bool containsCheckedRole = std::any_of(roles.begin(), roles.end(),
                [this](int role) { return role == Qt::CheckStateRole; });
            if (!containsCheckedRole)
                return;

            const auto rowCount = currentModel->rowCount();
            const auto checkedCount = getCheckedCount();
            if (rowCount == checkedCount &&  rowCount)
                m_checkableHeader->setCheckState(Qt::Checked);
            else if (!checkedCount)
                m_checkableHeader->setCheckState(Qt::Unchecked);
            else
                m_checkableHeader->setCheckState(Qt::PartiallyChecked);
        };

    connect(currentModel, &QAbstractItemModel::dataChanged, this, dataChangedHandler);
    dataChangedHandler(QModelIndex(), QModelIndex(), QVector<int>() << Qt::CheckStateRole);
}

void QnSimpleSelectionTableView::handleHeaderCheckedStateChanged(Qt::CheckState state)
{
    const auto currentModel = model();
    if (!currentModel)
        return;

    if (state == Qt::PartiallyChecked)
        return; // It was changed by manipulation with rows

    const int rowCount = currentModel->rowCount();
    for (int row = 0; row != rowCount; ++row)
    {
        const auto index = currentModel->index(row, m_checkboxColumn);
        currentModel->setData(index, state, Qt::CheckStateRole);
    }
}

void QnSimpleSelectionTableView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space)
        handleSpacePressed();

    base_type::keyPressEvent(event);
}

void QnSimpleSelectionTableView::setModel(QAbstractItemModel *newModel)
{
    const auto currentModel = model();
    if (currentModel == newModel)
        return;

    if (currentModel)
        disconnect(currentModel);

    base_type::setModel(newModel);

    setupHeader();
}

void QnSimpleSelectionTableView::handleClicked(const QModelIndex& index)
{
    const auto currentModel = model();
    if (!currentModel || !index.isValid())
        return;

    const auto checkState = isChecked(index) ? Qt::Unchecked : Qt::Checked;
    currentModel->setData(checkboxIndex(index), checkState, Qt::CheckStateRole);
}

void QnSimpleSelectionTableView::handleSpacePressed()
{
    const auto currentModel = model();
    if (!currentModel)
        return;

    const auto indexes = selectedIndexes();
    const bool anyUnchecked = std::any_of(indexes.begin(), indexes.end(),
        [this](const QModelIndex& index) { return !isChecked(index); });

    const auto newState = anyUnchecked ? Qt::Checked : Qt::Unchecked;
    for (const auto index: indexes)
        currentModel->setData(checkboxIndex(index), newState);
}

QModelIndex QnSimpleSelectionTableView::checkboxIndex(const QModelIndex& index)
{
    const auto currentModel = model();
    return currentModel && index.isValid()
        ? currentModel->index(index.row(), m_checkboxColumn)
        : QModelIndex();
}

bool QnSimpleSelectionTableView::isChecked(const QModelIndex& index)
{
    const auto currentModel = model();
    if (!currentModel)
        return false;

    const auto state = currentModel->data(checkboxIndex(index), Qt::CheckStateRole).toInt();

    return state != Qt::Unchecked;
}
