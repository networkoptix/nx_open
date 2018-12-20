#include "simple_selection_table_view.h"

#include <QtGui/QKeyEvent>

#include <nx/vms/client/desktop/common/widgets/checkable_header_view.h>

namespace nx::vms::client::desktop {

SimpleSelectionTableView::SimpleSelectionTableView(QWidget* parent):
    base_type(parent)
{
    connect(this, &SimpleSelectionTableView::clicked,
        this, &SimpleSelectionTableView::handleClicked);
}

SimpleSelectionTableView::~SimpleSelectionTableView()
{
}

void SimpleSelectionTableView::setCheckboxColumn(int column, bool checkboxInHeader)
{
    m_checkboxColumn = column;

    if (!checkboxInHeader)
    {
        m_checkableHeader = nullptr;
        setHorizontalHeader(new QHeaderView(Qt::Horizontal, this));
    }
    else
    {
        m_checkableHeader = new CheckableHeaderView(column, this);
        setHorizontalHeader(m_checkableHeader);
        setupHeader();
    }
}

int SimpleSelectionTableView::getCheckedCount() const
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

void SimpleSelectionTableView::setupHeader()
{
    const auto currentModel = model();
    if (!currentModel || !m_checkableHeader)
        return;

    connect(m_checkableHeader, &CheckableHeaderView::checkStateChanged,
        this, &SimpleSelectionTableView::handleHeaderCheckedStateChanged);

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

void SimpleSelectionTableView::handleHeaderCheckedStateChanged(Qt::CheckState state)
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
        if (currentModel->flags(index).testFlag(Qt::ItemIsUserCheckable))
            currentModel->setData(index, state, Qt::CheckStateRole);
    }
}

void SimpleSelectionTableView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space)
    {
        handleSpacePressed();
        event->accept();
        return;
    }

    base_type::keyPressEvent(event);
}

void SimpleSelectionTableView::setModel(QAbstractItemModel *newModel)
{
    const auto currentModel = model();
    if (currentModel == newModel)
        return;

    if (currentModel)
        disconnect(currentModel);

    base_type::setModel(newModel);

    setupHeader();
}

void SimpleSelectionTableView::handleClicked(const QModelIndex& index)
{
    const auto currentModel = model();
    if (!currentModel || !index.isValid())
        return;

    if (index == checkboxIndex(index))
        return; //< Click will be handled by base class implementation.

    if (!checkboxIndex(index).flags().testFlag(Qt::ItemIsUserCheckable))
        return;

    const auto newCheckState = isChecked(index) ? Qt::Unchecked : Qt::Checked;
    currentModel->setData(checkboxIndex(index), newCheckState, Qt::CheckStateRole);
}

void SimpleSelectionTableView::handleSpacePressed()
{
    const auto currentModel = model();
    if (!currentModel)
        return;

    const auto indexes = selectedIndexes();
    if (indexes.empty())
        return;

    const bool anyUnchecked = std::any_of(indexes.begin(), indexes.end(),
        [this](const QModelIndex& index) { return !isChecked(index); });

    const auto newState = anyUnchecked ? Qt::Checked : Qt::Unchecked;
    for (const auto index: indexes)
        currentModel->setData(checkboxIndex(index), newState, Qt::CheckStateRole);
}

QModelIndex SimpleSelectionTableView::checkboxIndex(const QModelIndex& index)
{
    const auto currentModel = model();
    return currentModel && index.isValid()
        ? currentModel->index(index.row(), m_checkboxColumn)
        : QModelIndex();
}

bool SimpleSelectionTableView::isChecked(const QModelIndex& index)
{
    const auto currentModel = model();
    if (!currentModel)
        return false;

    const auto state = currentModel->data(checkboxIndex(index), Qt::CheckStateRole).toInt();

    return state != Qt::Unchecked;
}

} // namespace nx::vms::client::desktop
