#include "simple_selection_table_view.h"

#include <QtGui/QKeyEvent>

#include <nx/vms/client/desktop/common/widgets/checkable_header_view.h>
#include <nx/vms/client/desktop/common/utils/item_view_utils.h>

namespace nx::vms::client::desktop {

SimpleSelectionTableView::SimpleSelectionTableView(QWidget* parent):
    base_type(parent)
{
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

    const auto isCheckable =
        [column](const QModelIndex& index) -> bool
        {
            auto flags = index.siblingAtColumn(column).flags();
            return flags.testFlag(Qt::ItemIsUserCheckable)
                && flags.testFlag(Qt::ItemIsEnabled);
        };

    item_view_utils::setupDefaultAutoToggle(this, m_checkboxColumn, isCheckable);
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

            int rowCount = 0;
            for (int row = 0; row < currentModel->rowCount(); ++row)
            {
                if (currentModel->index(row, m_checkboxColumn).flags().testFlag(Qt::ItemIsEnabled))
                    ++rowCount;
            }

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

void SimpleSelectionTableView::setModel(QAbstractItemModel* newModel)
{
    const auto currentModel = model();
    if (currentModel == newModel)
        return;

    if (currentModel)
        disconnect(currentModel);

    base_type::setModel(newModel);

    setupHeader();
}

void SimpleSelectionTableView::keyPressEvent(QKeyEvent* event)
{
    switch(event->key())
    {
        case Qt::Key_Tab:
        case Qt::Key_Enter:
        case Qt::Key_Return:
            if (state() != EditingState)
            {
                event->ignore();
                return;
            }
            break;
        default:
            break;
    }

    base_type::keyPressEvent(event);
}

} // namespace nx::vms::client::desktop
