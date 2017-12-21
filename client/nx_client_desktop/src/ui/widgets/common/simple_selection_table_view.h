#pragma once

#include <ui/widgets/common/table_view.h>

class QnCheckBoxedHeaderView;

class QnSimpleSelectionTableView: public QnTableView
{
    Q_OBJECT
    using base_type = QnTableView;

public:
    QnSimpleSelectionTableView(QWidget* parent = nullptr);

    ~QnSimpleSelectionTableView();

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
    QnCheckBoxedHeaderView* m_checkableHeader = nullptr;
    int m_checkboxColumn = 0;
};
