#ifndef QN_TREE_VIEW_H
#define QN_TREE_VIEW_H

#include <QtCore/QBasicTimer>
#include <QtGui/QTreeView>

/**
 * This class fixes some bugs in <tt>QTreeView</tt> related to drag and drop
 * handling when being embedded into a <tt>QGraphicsProxyWidget</tt>.
 * 
 * It also adds several features that the QTreeView is lacking.
 */
class QnTreeView: public QTreeView {
    Q_OBJECT;
public:
    QnTreeView(QWidget *parent = NULL);

    virtual ~QnTreeView();

signals:
    /**
     * This signal is emitted whenever the user presses enter on one of the
     * tree's items.
     * 
     * \param index                     Index of the item. Is guaranteed to be valid.
     */
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
