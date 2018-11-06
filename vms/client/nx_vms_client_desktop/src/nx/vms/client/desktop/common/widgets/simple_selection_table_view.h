#pragma once

#include <nx/vms/client/desktop/common/widgets/table_view.h>

namespace nx::vms::client::desktop {

class CheckableHeaderView;

class SimpleSelectionTableView: public TableView
{
    Q_OBJECT
    using base_type = TableView;

public:
    SimpleSelectionTableView(QWidget* parent = nullptr);

    virtual ~SimpleSelectionTableView() override;

    void setCheckboxColumn(int column, bool checkboxInHeader = true);

    int getCheckedCount() const;

public:
    virtual void keyPressEvent(QKeyEvent* event) override;

    virtual void setModel(QAbstractItemModel *newModel) override;

private:
    void setupHeader();

    void handleClicked(const QModelIndex& index);

    void handleSpacePressed();

    QModelIndex checkboxIndex(const QModelIndex& index);

    bool isChecked(const QModelIndex& index);

    void handleHeaderCheckedStateChanged(Qt::CheckState state);

private:
    CheckableHeaderView* m_checkableHeader = nullptr;
    int m_checkboxColumn = 0;
};

} // namespace nx::vms::client::desktop
