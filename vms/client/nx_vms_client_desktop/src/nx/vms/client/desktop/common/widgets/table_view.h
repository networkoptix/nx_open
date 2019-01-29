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

    /** Sometimes we need to disable default selection behavior by space key and handle it manually. */
    bool isDefaultSpacePressIgnored() const;
    void setDefaultSpacePressIgnored(bool isIgnored);

    ItemViewHoverTracker* hoverTracker() const;

    // Takes ownership of delegate.
    void setPersistentDelegateForColumn(int column, QAbstractItemDelegate* delegate);

signals:
    /**
     * This signal is emitted whenever the user presses space on one of the
     * tree's items.
     *
     * \param index Index of the item. Is guaranteed to be valid.
     */
    void spacePressed(const QModelIndex& index);

    /**
    * This signal is emitted from selectionCommand whenever selection change is about to occur
    */
    void selectionChanging(QItemSelectionModel::SelectionFlags selectionFlags,
        const QModelIndex& index, const QEvent* event) const;

protected:
    virtual bool edit(const QModelIndex& index, EditTrigger trigger, QEvent* event) override;
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;
    virtual void keyPressEvent(QKeyEvent* event) override;
    virtual QItemSelectionModel::SelectionFlags selectionCommand(
        const QModelIndex& index, const QEvent* event = nullptr) const override;

private:
    QRect rowRect(int row) const;
    void openEditorsForColumn(int column, int firstRow, int lastRow);

private:
    ItemViewHoverTracker* const m_tracker = nullptr;
    bool m_isDefauldSpacePressIgnored = false;
    using ColumnDelegateHash = QHash<int, QPointer<QAbstractItemDelegate>>;
    ColumnDelegateHash m_delegates;
};

} // namespace nx::vms::client::desktop
