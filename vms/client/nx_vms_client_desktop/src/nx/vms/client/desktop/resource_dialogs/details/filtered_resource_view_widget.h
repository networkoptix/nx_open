// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtWidgets/QWidget>
#include <QtCore/QModelIndexList>
#include <QtWidgets/QAbstractItemView>

class QAbstractItemModel;
class QAbstractItemDelegate;
class QHeaderView;
class QnIndents;

namespace Ui { class FilteredResourceViewWidget; }

namespace nx::vms::client::desktop {

class TreeView;
class FilteredResourceProxyModel;
class ItemViewHoverTracker;

class FilteredResourceViewWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

    Q_PROPERTY(QString invalidMessage READ invalidMessage WRITE setInvalidMessage)
    Q_PROPERTY(QString infoMessage READ infoMessage WRITE setInfoMessage)

public:
    FilteredResourceViewWidget(QWidget* parent = nullptr);
    virtual ~FilteredResourceViewWidget() override;

    /**
     * @return Pointer to the source item model if one has been set.
     */
    QAbstractItemModel* sourceModel() const;

    /**
     * @param sourceIndex Valid model index that belongs to the source item model.
     * @return Model index that belongs to a model attached to the tree view if corresponding item
     *     currently isn't filtered out, default constructed <tt>QModelIndex</tt> otherwise.
     */
    QModelIndex toViewIndex(const QModelIndex& sourceIndex) const;

    /**
     * Sets source item model. Single column data model expected regardless of whether the model
     * hierarchical or flat. Header data provided by model won't be displayed.
     * @param Valid pointer to the source item model, ownership of model doesn't transferred to the
     *      resource view widget.
     */
    void setSourceModel(QAbstractItemModel* model);

    /**
     * @return Pointer to the header view of encapsulated tree view.
     */
    QHeaderView* treeHeaderView() const;

    /**
     * @return Item delegate used by tree view.
     */
    QAbstractItemDelegate* itemDelegate() const;

    /**
     * Sets item delegate to the tree view, ownership of delegate doesn't transferred.
     */
    void setItemDelegate(QAbstractItemDelegate* itemDelegate);

    /**
     * Allows to disable tree indentation for right representation of flat (list) data models.
     */
    void setUseTreeIndentation(bool useTreeIndentation);

    /**
     * Allows to alter item contents side margins. Default indentation is
     * <tt>style::Metrics::kDefaultTopLevelMargin</tt> both for left and right side.
     * @param indents New value for indents.
     */
    void setSideIndentation(const QnIndents& indents);

    /**
     * Accessor for the same named method of the encapsulated tree view.
     */
    void setUniformRowHeights(bool uniform);

    /**
     * Sets tree view selection mode.
     */
    void setSelectionMode(QAbstractItemView::SelectionMode selectionMode);

    /**
     * @return List of currently selected source model indexes (in terms of item selection model,
     *     not to be confused with checked items).
     */
    QModelIndexList selectedIndexes() const;

    /**
     * @param index Valid source model index.
     * @return Visual rect in the coordinate system of this widget.
     */
    QRect itemRect(const QModelIndex& index) const;

    /**
     * @return Clear current selection if tree view has one (in terms of item selection model,
     *     not to be confused with checked items).
     */
    void clearSelection();

    /**
     * Clears filter input and resets resource view to the initial state.
     */
    void clearFilter();

    /**
     * @return Current source model index (in terms of item selection model).
     */
    QModelIndex currentIndex() const;

    /**
     * @return True if source model contains no data or source model hasn't been set.
     */
    bool isEmpty() const;

    /**
     * @param enabled Enabled state which will be applied both to the encapsulated tree view
     *     and filter line edit.
     */
    void setItemViewEnabled(bool enabled);

    /**
     * @return Pointer to the header widget which will be located above filter input (not be
     *     confused with tree view header). By default there is no header widget.
     */
    QWidget* headerWidget() const;

    /**
     * Sets header widget which will be located above filter input.
     * @param widget Valid pointer to the QWidget descendant, ownership will be transferred
     *     to the corresponding layout. Passing nullptr results in removing existing header widget.
     *     It will be deleted as control will return to the event loop.
     */
    void setHeaderWidget(QWidget* widget);

    /**
     * @return Whether previously set header widget currently displayed or not. Header widget is
     *     in visible state right after adding.
     */
    bool headerWidgetVisible() const;

    /**
     * Sets whether previously set header widget should be displayed or not. Calling this function
     * doesn't affect header widget in any other way.
     */
    void setHeaderWidgetVisible(bool visible);

    /**
     * @return Pointer to the footer widget which will be located on the panel below tree view.
     *     By default there is no footer widget.
     */
    QWidget* footerWidget() const;

    /**
     * Sets footer widget which will be located on the panel below tree view. If no widget
     * set, panel won't be displayed either.
     * @param widget Valid pointer to the QWidget descendant, ownership will be transferred
     *     to the corresponding layout. Passing nullptr results in removing existing header widget.
     *     It will be deleted as control will return to the event loop.
     */
    void setFooterWidget(QWidget* widget);

    /**
     * @return Whether previously set footer widget currently displayed or not. Footer widget is
     *     in visible state right after adding.
     */
    bool footerWidgetVisible() const;

    /**
     * Sets whether previously set footer widget should be displayed or not. Calling this function
     * doesn't affect footer widget in any other way.
     */
    void setFooterWidgetVisible(bool visible);

    /**
     * Expands necessary and sufficient set of node items within tree view to make all required
     * leaf items (see <tt>setVisibleItemPredicate()</tt>) become exposed.
     */
    void makeRequiredItemsVisible() const;

    using VisibleItemPredicate = std::function<bool(const QModelIndex&)>;
    /**
     * Sets predicate which evaluates whether leaf item with given model index should be visible or
     * not on the first show. If item should be visible, then all its parents will be expanded.
     */
    void setVisibleItemPredicate(const VisibleItemPredicate& visibleItemPredicate);

    /**
     * @return Instance of item view hover tracker attached to the encapsulated tree view.
     *     Hover tracker won't be instantiated if this method will never called.
     */
    ItemViewHoverTracker* itemViewHoverTracker();

    QString invalidMessage() const;
    void setInvalidMessage(const QString& text);
    void clearInvalidMessage();

    QString infoMessage() const;
    void setInfoMessage(const QString& text);
    void clearInfoMessage();

signals:

    /**
     * Signal is emitted when the mouse cursor enters or leaves an item boundaries.
     * @param sourceIndex Source model index of item under mouse cursor, invalid QModelIndex if
     *     there is no such item.
     */
    void itemHovered(const QModelIndex& sourceIndex);

    /**
     * Signal is emitted when item is clicked by left mouse button.
     * @param sourceIndex Source model index of item that was clicked.
     */
    void itemClicked(const QModelIndex& sourceIndex);

    /**
     * Signal is emitted when current item changed (in terms of item selection model).
     * @param current Source model index of new current item.
     * @param previous Source model index of previous current item.
     */
    void currentItemChanged(const QModelIndex& current, const QModelIndex& previous);

    /**
     * Signal is emitted whenever the item selection changes.
     */
    void selectionChanged();

protected:
    virtual void leaveEvent(QEvent* event) override;

private:
    TreeView* treeView() const;

    void setupSignals();
    void setupFilter();

private:
    const std::unique_ptr<Ui::FilteredResourceViewWidget> ui;
    const std::unique_ptr<FilteredResourceProxyModel> m_filterProxyModel;
    QWidget* m_headerWidget = nullptr;
    QWidget* m_footerWidget = nullptr;
    ItemViewHoverTracker* m_itemViewHoverTracker = nullptr;
    QVector<QPersistentModelIndex> m_expandedSourceIndexes;
    VisibleItemPredicate m_visibleItemPredicate;
};

} // namespace nx::vms::client::desktop
