// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    virtual void setModel(QAbstractItemModel* newModel) override;

protected:
    virtual void keyPressEvent(QKeyEvent* event) override;

private:
    void setupHeader();
    void handleHeaderCheckedStateChanged(Qt::CheckState state);

private:
    CheckableHeaderView* m_checkableHeader = nullptr;
    int m_checkboxColumn = 0;
};

} // namespace nx::vms::client::desktop
