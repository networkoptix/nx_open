#pragma once

#include <QtGui/QMouseEvent>

#include "table_view.h"

namespace nx::vms::client::desktop {

/*
* This class extend TableView for better checkboxes behavior
*  Checkboxes are kept synchronized with selection
*/
class CheckableTableView: public TableView
{
    Q_OBJECT

    /* Index of column that contains checkboxes */
    Q_PROPERTY(int checkboxColumn READ checkboxColumn WRITE setCheckboxColumn)

    /*
    This property determines how dataChanged signal is emitted when selection changes.
    When false, dataChanged is emitted for every range which selection was changed.
    When true, dataChanged is emitted once for all rows.
    */
    Q_PROPERTY(bool wholeModelChange READ wholeModelChange WRITE setWholeModelChange)

    using base_type = TableView;

public:
    explicit CheckableTableView(QWidget* parent = nullptr);

    virtual void reset() override;

    int checkboxColumn() const { return m_checkboxColumn; }
    void setCheckboxColumn(int value) { m_checkboxColumn = value; }

    bool wholeModelChange() const { return m_wholeModelChange; }
    void setWholeModelChange(bool value) { m_wholeModelChange = value; }

protected:
    virtual void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) override;
    virtual QItemSelectionModel::SelectionFlags selectionCommand(const QModelIndex& index, const QEvent* event) const override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void synchronizeWithModel();
    void modelDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles);

private:
    int m_checkboxColumn = 0;
    bool m_wholeModelChange = false;

    QMetaObject::Connection m_dataChangedConnection;
    bool m_synchronizingWithModel = false;

    mutable bool m_mousePressedOnCheckbox = false;
};

} // namespace nx::vms::client::desktop
