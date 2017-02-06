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
class QnTreeView: public QTreeView
{
    Q_OBJECT
    typedef QTreeView base_type;

public:
    explicit QnTreeView(QWidget* parent = nullptr);
    virtual ~QnTreeView();

    using base_type::rowHeight;

    /** Sometimes we need to disable default selection behavior by space key and handle it manually. */
    bool ignoreDefaultSpace() const;
    void setIgnoreDefaultSpace(bool value);

    /** Fix to allow drop something in the space left of the node icon. Default: true. */
    bool dropOnBranchesAllowed() const;
    void setDropOnBranchesAllowed(bool value);

    virtual QRect visualRect(const QModelIndex& index) const override;

signals:
    /**
     * This signal is emitted whenever the user presses enter on one of the
     * tree's items.
     *
     * \param index                     Index of the item. Is guaranteed to be valid.
     */
    void enterPressed(const QModelIndex& index);

    /**
     * This signal is emitted whenever the user presses space on one of the
     * tree's items.
     *
     * \param index                     Index of the item. Is guaranteed to be valid.
     */
    void spacePressed(const QModelIndex& index);

protected:
    virtual void keyPressEvent(QKeyEvent* event) override;
    virtual void dragMoveEvent(QDragMoveEvent* event) override;
    virtual void dragLeaveEvent(QDragLeaveEvent* event) override;
    virtual void dropEvent(QDropEvent* event) override;
    virtual void timerEvent(QTimerEvent* event) override;
    virtual void scrollContentsBy(int dx, int dy) override;
    virtual QSize viewportSizeHint() const override;

private:
    QBasicTimer m_openTimer;
    QPoint m_dragMovePos;

    bool m_ignoreDefaultSpace;
    bool m_dropOnBranchesAllowed;
    bool m_inDragDropEvent;
};


#endif // QN_TREE_VIEW_H
