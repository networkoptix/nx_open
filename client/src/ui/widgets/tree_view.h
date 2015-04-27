#ifndef QN_TREE_VIEW_H
#define QN_TREE_VIEW_H

#include <QtCore/QBasicTimer>
#include <QtWidgets/QTreeView>

/**
 * This class fixes some bugs in <tt>QTreeView</tt> related to drag and drop
 * handling when being embedded into a <tt>QGraphicsProxyWidget</tt>.
 * 
 * It also adds several features that the QTreeView is lacking.
 */
class QnTreeView: public QTreeView {
    Q_OBJECT

    typedef QTreeView base_type;
public:
    QnTreeView(QWidget *parent = NULL);

    virtual ~QnTreeView();

    int rowHeight(const QModelIndex &index) const;

signals:
    /**
     * This signal is emitted whenever the user presses enter on one of the
     * tree's items.
     * 
     * \param index                     Index of the item. Is guaranteed to be valid.
     */
    void enterPressed(const QModelIndex &index);

    /**
     * This signal is emitted whenever the user presses space on one of the
     * tree's items.
     *
     * \param index                     Index of the item. Is guaranteed to be valid.
     */
    void spacePressed(const QModelIndex &index);
protected:
    virtual void keyPressEvent(QKeyEvent *event) override;
    virtual void dragMoveEvent(QDragMoveEvent *event) override;
    virtual void dragLeaveEvent(QDragLeaveEvent *event) override;
    virtual void timerEvent(QTimerEvent *event) override;

    virtual bool edit(const QModelIndex &index, EditTrigger trigger, QEvent *event) override;
    virtual void closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint) override;

private:
    QBasicTimer m_openTimer;
    QPoint m_dragMovePos;

    /* Flag that an element is edited right now. Workaround for Qt bug: state() is not always Editing. */
    bool m_editorOpen;
};


#endif // QN_TREE_VIEW_H
