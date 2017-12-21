#pragma once

#include <QtCore/QEvent>
#include <QtWidgets/QTableView>

class QnItemViewHoverTracker;

/**
 * This class fixes a bug in <tt>QTableView</tt> related to editor triggers
 *  and implements entire row hovering behavior.
 */
class QnTableView : public QTableView
{
    Q_OBJECT

    typedef QTableView base_type;

public:
    explicit QnTableView(QWidget* parent = nullptr);
    virtual ~QnTableView();

    virtual QSize viewportSizeHint() const override;
    virtual void setModel(QAbstractItemModel* newModel) override;

    QnItemViewHoverTracker* hoverTracker() const;

    // Takes ownership of delegate
    void setPersistentDelegateForColumn(int column, QAbstractItemDelegate* delegate);

protected:
    virtual bool edit(const QModelIndex& index, EditTrigger trigger, QEvent* event) override;
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;

private:
    QRect rowRect(int row) const;

    void openEditorsForColumn(int column, int firstRow, int lastRow);

private:
    QnItemViewHoverTracker* m_tracker;

    using ColumnDelegateHash = QHash<int, QPointer<QAbstractItemDelegate>>;
    ColumnDelegateHash m_delegates;
};
