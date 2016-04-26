#pragma once

#include <QtCore/QEvent>
#include <QtWidgets/QTableView>

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

    QModelIndex mouseIndex() const { return m_lastModelIndex; }

signals:
    void hoverEnter(const QModelIndex& index);
    void hoverLeave(const QModelIndex& index);
    void rowEnter(int row);
    void rowLeave(int row);

protected:
    virtual bool edit(const QModelIndex& index, EditTrigger trigger, QEvent* event) override;
    virtual bool event(QEvent* event) override;

    virtual void hoverEvent(QHoverEvent* event);

private:
    void changeHover(const QModelIndex& index);
    QRect rowRect(int row) const;

private:
    QModelIndex m_lastModelIndex;
};
