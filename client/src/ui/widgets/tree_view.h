#ifndef QN_TREE_VIEW_H
#define QN_TREE_VIEW_H

#include <QTreeView>
#include <QBasicTimer>

/**
 * This class fixes some bugs in <tt>QTreeView</tt> related to drag and drop
 * handling when embedded into a <tt>QGraphicsProxyWidget</tt>.
 */
class QnTreeView: public QTreeView {
    Q_OBJECT;
public:
    QnTreeView(QWidget *parent = NULL);

    virtual ~QnTreeView();

signals:
    void enterPressed(const QModelIndex &index);

protected:
    virtual void keyPressEvent(QKeyEvent *event) override;
    virtual void dragMoveEvent(QDragMoveEvent *event) override;
    virtual void dragLeaveEvent(QDragLeaveEvent *event) override;
    virtual void timerEvent(QTimerEvent *event) override;

private:
    QBasicTimer m_openTimer;
    QPoint m_dragMovePos;
};


#endif // QN_TREE_VIEW_H
