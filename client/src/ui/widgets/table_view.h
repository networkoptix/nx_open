#ifndef TABLE_VIEW_H
#define TABLE_VIEW_H

#include <QtGui/QTableView>

/** The event-forwarder is needed to forward the entered() event from the header and scrool bars
*   controls to the widgets. The widgets set the cursor to the default when the header control is entered.
*/

class ItemViewEventForwarder: public QObject
{
    Q_OBJECT

signals:
    // Is emitted when an event of type QEvent::Enter is received.
    void entered();

public:
    virtual bool eventFilter(QObject *aWatched, QEvent *aEvent);
};

/**
 * This class fixes a bug in <tt>QTableView</tt> related to editor triggers.
 *
 */
class QnTableView : public QTableView
{
    Q_OBJECT

    typedef QTableView base_type;
public:

    explicit QnTableView(QWidget *parent = 0);
    virtual ~QnTableView();

protected:
    virtual bool edit(const QModelIndex &index, EditTrigger trigger, QEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void leaveEvent(QEvent * event) override;
private slots:
    void resetCursor();
private:
    QModelIndex m_lastMouseModelIndex;
    ItemViewEventForwarder m_eventForwarder;
};

#endif // TABLE_VIEW_H
