#pragma once

#include <QtCore/QHash>
#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtWidgets/QTableView>

namespace nx::vms::client::desktop {

class ItemViewHoverTracker;

/**
 * This class fixes a bug in <tt>QTableView</tt> related to editor triggers
 *  and implements entire row hovering behavior.
 */
class TableView: public QTableView
{
    Q_OBJECT
    using base_type = QTableView;

public:
    explicit TableView(QWidget* parent = nullptr);
    virtual ~TableView() override = default;

    virtual QSize viewportSizeHint() const override;
    virtual void setModel(QAbstractItemModel* newModel) override;

    ItemViewHoverTracker* hoverTracker() const;

    // Takes ownership of delegate.
    void setPersistentDelegateForColumn(int column, QAbstractItemDelegate* delegate);

protected:
    virtual bool edit(const QModelIndex& index, EditTrigger trigger, QEvent* event) override;
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;

private:
    QRect rowRect(int row) const;

    void openEditorsForColumn(int column, int firstRow, int lastRow);

private:
    ItemViewHoverTracker* const m_tracker = nullptr;

    using ColumnDelegateHash = QHash<int, QPointer<QAbstractItemDelegate>>;
    ColumnDelegateHash m_delegates;
};

} // namespace nx::vms::client::desktop
