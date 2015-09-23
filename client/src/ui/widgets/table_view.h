#ifndef TABLE_VIEW_H
#define TABLE_VIEW_H

#include <QtWidgets/QTableView>

/**
 * This class fixes a bug in <tt>QTableView</tt> related to editor triggers.
 */
class QnTableView : public QTableView
{
    Q_OBJECT

    typedef QTableView base_type;
public:

    explicit QnTableView(QWidget *parent = 0);
    virtual ~QnTableView();

    QModelIndex mouseIndex() const { return m_lastMouseModelIndex; }

protected:
    virtual bool edit(const QModelIndex &index, EditTrigger trigger, QEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void leaveEvent(QEvent * event) override;

private slots:
    void resetCursor();

private:
    QModelIndex m_lastMouseModelIndex;
};

#endif // TABLE_VIEW_H
