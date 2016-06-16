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

    QnItemViewHoverTracker* hoverTracker() const;

protected:
    virtual bool edit(const QModelIndex& index, EditTrigger trigger, QEvent* event) override;
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;

private:
    QRect rowRect(int row) const;

private:
    QnItemViewHoverTracker* m_tracker;
};
