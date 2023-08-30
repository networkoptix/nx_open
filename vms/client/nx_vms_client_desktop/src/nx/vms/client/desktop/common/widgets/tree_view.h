// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QBasicTimer>
#include <QtWidgets/QTreeView>

namespace nx::vms::client::desktop {

/**
 * Class adds several features that the QTreeView is lacking.
 */
class TreeView: public QTreeView
{
    Q_OBJECT
    using base_type = QTreeView;

public:
    explicit TreeView(QWidget* parent = nullptr);
    virtual ~TreeView() override = default;

    using base_type::sizeHintForColumn;

    /**
     * Disables default selection behavior performed by pressing space key. spacePressed signal
     * is still emitted and may be processed to implement custom behavior.
     */
    bool isDefaultSpacePressIgnored() const;
    void setDefaultSpacePressIgnored(bool isIgnored);

    void setEnterKeyEventIgnored(bool ignored);

    /**
     * Allow editing with multiple selection. By default, editing with multiple
     * selection is not allowed.
     */
    void setMultiSelectionEditAllowed(bool value);
    bool isMultiSelectionEditAllowed() const;

    /**
     * Allows drops in the space by the left of the node icon.
     * @returns True by default, unless explicitly specified other.
     */
    bool dropOnBranchesAllowed() const;
    void setDropOnBranchesAllowed(bool value);

    virtual QRect visualRect(const QModelIndex& index) const override;

    /**
     * Filter predicate. May be used for blocking interaction with the item via double click.
     */
    using ConfirmExpandDelegate = std::function<bool(const QModelIndex& index)>;
    void setConfirmExpandDelegate(ConfirmExpandDelegate value);

    /**
     * If tree view lays within another scroll area as part of composition, it should ignore
     * received wheel events in order to higher level scroll area keep receiving them.
     * It's relevant to events which have phase, first of all.
     */
    void setIgnoreWheelEvents(bool ignore);

    /**
     * Collapses item at given index and all the expandable items within subtree with root at given
     * <tt>index</tt>.
     */
    void collapseRecursively(const QModelIndex& index);

    /**
     * Sets all the expandable items within subtree with root at <tt>index</tt> to either
     * collapsed or expanded, depending on the value of <tt>expanded</tt>.
     */
    void setExpandedRecursively(const QModelIndex& index, bool expanded);

    /**
     * Enables custom sizeHint, which is calculated from the number of tree elements.
     * By default, sizeHint is calculated from setting sizePolicy.
     */
    void setCustomSizeHint(bool isUsed);

    virtual QSize sizeHint() const override;

signals:

    /**
     * Emitted when user presses enter key while tree view has non null current index.
     * @param index Tree view current index, guaranteed to be valid.
     */
    void enterPressed(const QModelIndex& index);

    /**
     * Emitted when user presses space key while tree view has non null current index.
     * @param index Tree view current index, guaranteed to be valid.
     */
    void spacePressed(const QModelIndex& index);

    /**
     * Emitted right after selectionCommand has been executed, before return of result.
     */
    void selectionChanging(QItemSelectionModel::SelectionFlags selectionFlags,
        const QModelIndex& index, const QEvent* event) const;

    /** Emitted when the widget gets focus. */
    void gotFocus(Qt::FocusReason reason);

    /** Emitted when the widget losts focus. */
    void lostFocus(Qt::FocusReason reason);

protected:
    virtual bool edit(
        const QModelIndex& index, QAbstractItemView::EditTrigger trigger, QEvent* event) override;
    virtual void keyPressEvent(QKeyEvent* event) override;
    virtual void dragMoveEvent(QDragMoveEvent* event) override;
    virtual void dragLeaveEvent(QDragLeaveEvent* event) override;
    virtual void dropEvent(QDropEvent* event) override;
    virtual void focusInEvent(QFocusEvent* event) override;
    virtual void focusOutEvent(QFocusEvent* event) override;
    virtual void timerEvent(QTimerEvent* event) override;
    virtual void mouseDoubleClickEvent(QMouseEvent* event) override;
    virtual void wheelEvent(QWheelEvent* event) override;
    virtual void scrollContentsBy(int dx, int dy) override;
    virtual QItemSelectionModel::SelectionFlags selectionCommand(
        const QModelIndex& index, const QEvent* event = nullptr) const override;

private:
    int calculateHeight(const QModelIndex& index) const;

private:
    QBasicTimer m_openTimer;
    QPoint m_dragMovePos;
    bool m_isDefauldSpacePressIgnored = false;
    bool m_enterKeyEventIgnored = true;
    bool m_dropOnBranchesAllowed = true;
    bool m_inDragDropEvent = false;
    bool m_ignoreWheelEvents = false;
    bool m_useCustomSizeHint = false;
    bool m_allowMultiSelectionEdit = false;
    ConfirmExpandDelegate m_confirmExpand;
};

} // namespace nx::vms::client::desktop
