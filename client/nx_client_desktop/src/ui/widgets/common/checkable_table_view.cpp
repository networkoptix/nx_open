#include "checkable_table_view.h"

#include <QtCore/QVariant>
#include <QtCore/QVector>
#include <QtCore/QScopedValueRollback>

#include <client/client_globals.h>

#include <utils/common/delayed.h>

namespace
{
    const QVector<int> kCheckRoles({ Qt::CheckStateRole });

    QItemSelectionModel::SelectionFlags entireRow(QItemSelectionModel::SelectionFlags flags)
    {
        return (flags != QItemSelectionModel::NoUpdate) ? (flags | QItemSelectionModel::Rows) : QItemSelectionModel::NoUpdate;
    }
}

QnCheckableTableView::QnCheckableTableView(QWidget* parent):
    QnTableView(parent),
    m_checkboxColumn(0),
    m_wholeModelChange(false),
    m_synchronizingWithModel(false),
    m_mousePressedOnCheckbox(false)
{
}

void QnCheckableTableView::reset()
{
    /* Disable visual updates: */
    setUpdatesEnabled(false);
    bool syncRequired = false;

    /* Inherited reset: */
    base_type::reset();

    /* Disconnect previous model connection: */
    disconnect(m_dataChangedConnection);

    /* If model exists and is not empty we should synchronize selection with it: */
    if (auto dataModel = model())
    {
        syncRequired = dataModel->rowCount() > 0;
        if (syncRequired)
        {
            /* Defer synchronization past all further handing of modelReset signal: */
            const auto callback =
                [this]()
                {
                    /* Synchronize selection with model: */
                    synchronizeWithModel();

                    /* Re-enable visual updates and mark entire widget for update: */
                    setUpdatesEnabled(true);
                    update();
                };
            executeDelayedParented(callback, kDefaultDelay, this);
        }

        /* Connect to model's dataChanged: */
        m_dataChangedConnection = connect(dataModel, &QAbstractItemModel::dataChanged, this, &QnCheckableTableView::modelDataChanged);
    }

    /* Re-enable visual updates if synchronization wasn't queued, and mark entire widget for update: */
    if (!syncRequired)
    {
        setUpdatesEnabled(true);
        update();
    }
}

void QnCheckableTableView::synchronizeWithModel()
{
    if (auto dataModel = model())
    {
        QModelIndex selectedStart;
        QItemSelection selected;

        /* Obtain selected ranges from the model: */
        int rowCount = dataModel->rowCount();
        for (int row = 0; row < rowCount; ++row)
        {
            QModelIndex index = dataModel->index(row, m_checkboxColumn);
            if (index.data(Qt::CheckStateRole).toInt() == Qt::Checked)
            {
                /* Begin selected range if not begun already: */
                if (!selectedStart.isValid())
                    selectedStart = index;
            }
            else
            {
                if (selectedStart.isValid())
                {
                    /* End selected range and store it: */
                    selected.select(selectedStart, dataModel->index(row - 1, m_checkboxColumn));
                    selectedStart = QModelIndex();
                }
            }
        }

        /* End selected range if still open, and store it: */
        if (selectedStart.isValid())
            selected.select(selectedStart, dataModel->index(rowCount - 1, m_checkboxColumn));

        /* Perform batch clearing and selection: */
        QScopedValueRollback<bool> rollback(m_synchronizingWithModel, true);
        selectionModel()->select(selected, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
}

void QnCheckableTableView::modelDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles)
{
    if (auto dataModel = model())
    {
        /* If checkbox column was possibly affected: */
        if ((roles.empty() || roles.contains(Qt::CheckStateRole)) &&
            topLeft.column() <= m_checkboxColumn &&
            bottomRight.column() >= m_checkboxColumn)
        {
            QModelIndex selectedStart, deselectedStart;
            QItemSelection selected, deselected;

            /* Gather selected and deselected ranges: */
            for (int row = topLeft.row(); row <= bottomRight.row(); ++row)
            {
                QModelIndex index = dataModel->index(row, m_checkboxColumn);
                if (index.data(Qt::CheckStateRole).toInt() == Qt::Checked)
                {
                    /* Begin selected range if not begun already: */
                    if (!selectedStart.isValid())
                        selectedStart = index;

                    if (deselectedStart.isValid())
                    {
                        /* End deselected range and store it: */
                        deselected.select(deselectedStart, dataModel->index(row - 1, m_checkboxColumn));
                        deselectedStart = QModelIndex();
                    }
                }
                else
                {
                    /* Begin deselected range if not begun already: */
                    if (!deselectedStart.isValid())
                        deselectedStart = index;

                    if (selectedStart.isValid())
                    {
                        /* End selected range and store it: */
                        selected.select(selectedStart, dataModel->index(row - 1, m_checkboxColumn));
                        selectedStart = QModelIndex();
                    }
                }
            }

            /* End selected range if still open, and store it: */
            if (selectedStart.isValid())
                selected.select(selectedStart, dataModel->index(bottomRight.row(), m_checkboxColumn));

            /* End deselected range if still open, and store it: */
            if (deselectedStart.isValid())
                deselected.select(deselectedStart, dataModel->index(bottomRight.row(), m_checkboxColumn));

            /* Perform batch selection and deselection: */
            QScopedValueRollback<bool> rollback(m_synchronizingWithModel, true);
            selectionModel()->select(selected, QItemSelectionModel::Select | QItemSelectionModel::Rows);
            selectionModel()->select(deselected, QItemSelectionModel::Deselect | QItemSelectionModel::Rows);
        }
    }
}

void QnCheckableTableView::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    base_type::selectionChanged(selected, deselected);

    /* Skip synchronizing model with selection if we're synchronizing selection with model: */
    if (m_synchronizingWithModel)
        return;

    /* Keep model check states in sync with selection: */
    if (auto dataModel = model())
    {
        QSignalBlocker blocker(dataModel);

        bool b = true;

        /* Uncheck deselected: */
        if (!b)
        {
            for (auto range : deselected)
            {
                for (auto row = range.top(); row <= range.bottom(); ++row)
                    dataModel->setData(dataModel->index(row, m_checkboxColumn), Qt::Unchecked, Qt::CheckStateRole);
            }

        }

        /* Check selected: */
        if (!selected.isEmpty())
        {
            const auto range = selected.first();
            const auto row = range.top();
            const auto index = dataModel->index(row, m_checkboxColumn);
            const auto newValue = index.data(Qt::CheckStateRole) == Qt::Unchecked
                ? Qt::Checked
                : Qt::Unchecked;

            for (auto range: selected)
            {
                for (auto row = range.top(); row <= range.bottom(); ++row)
                {
                    const auto index = dataModel->index(row, m_checkboxColumn);
                    dataModel->setData(index, newValue, Qt::CheckStateRole);
                }
            }
        }

        blocker.unblock();

        /* Temporarily disconnect from model's dataChanged: */
        disconnect(m_dataChangedConnection);

        if (m_wholeModelChange)
        {
            /* Emit change signal for entire model: */
            emit dataModel->dataChanged(dataModel->index(0, m_checkboxColumn), dataModel->index(dataModel->rowCount() - 1, m_checkboxColumn), kCheckRoles);
        }
        else
        {
            /* Make a union of selected and deselected ranges: */
            QItemSelection changed(selected);
            changed.merge(deselected, QItemSelectionModel::Select);

            /* Emit change signal for each changed range model: */
            for (auto range : changed)
                emit dataModel->dataChanged(dataModel->index(range.top(), m_checkboxColumn), dataModel->index(range.bottom(), m_checkboxColumn), kCheckRoles);
        }

        /* Connect to model's dataChanged: */
        m_dataChangedConnection = connect(dataModel, &QAbstractItemModel::dataChanged, this, &QnCheckableTableView::modelDataChanged);
    }
}

QItemSelectionModel::SelectionFlags QnCheckableTableView::selectionCommand(const QModelIndex& index, const QEvent* event) const
{
    /* Avoid drag-selecting operations originating from checkbox column items: */
    if (m_mousePressedOnCheckbox)
        return QItemSelectionModel::NoUpdate;
    /*
    * m_mousePressedOnCheckbox boolean variable is set in this method after left mouse button is pressed
    *   on the checkbox column (and handled as selection command), and it is cleared in mouseReleaseEvent()
    *   when left mouse button is released.
    * It indicates the state when drag-selection operations must not be started, because we don't want the user
    *    to click on a checkbox, drag and select other rows (and deselect everything that was selected before).
    */

    /* If selection event is mouse button press on a checkbox column item: */
    if (event && event->type() == QEvent::MouseButtonPress && index.column() == m_checkboxColumn)
    {
        const QMouseEvent& mouseEvent = *static_cast<const QMouseEvent*>(event);
        if (mouseEvent.button() == Qt::LeftButton)
        {
            /* Create a new event with Ctrl-modifier: */
            QMouseEvent newMouseEvent(
                QEvent::MouseButtonPress,
                mouseEvent.localPos(), mouseEvent.windowPos(), mouseEvent.screenPos(),
                mouseEvent.button(), mouseEvent.buttons(),
                Qt::ControlModifier,
                mouseEvent.source());

            /* Register mouse button press originating from a checkbox column item: */
            m_mousePressedOnCheckbox = true;

            /* Call standard implementation with Ctrl-modifier, add entire row selection behavior: */
            return entireRow(base_type::selectionCommand(index, &newMouseEvent));
        }
    }

    /* Call standard implementation, add entire row selection behavior: */
    return entireRow(base_type::selectionCommand(index, event));
}

void QnCheckableTableView::mouseReleaseEvent(QMouseEvent* event)
{
    /* If left mouse button is released: */
    if (static_cast<const QMouseEvent*>(event)->button() == Qt::LeftButton)
        m_mousePressedOnCheckbox = false;

    base_type::mouseReleaseEvent(event);
}
