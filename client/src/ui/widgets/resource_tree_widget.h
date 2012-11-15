#ifndef RESOURCE_TREE_WIDGET_H
#define RESOURCE_TREE_WIDGET_H

#include <QtCore/QAbstractItemModel>

#include <QtGui/QWidget>
#include <QtGui/QItemSelectionModel>

#include <core/resource/resource_fwd.h>

class QnResourceTreeItemDelegate;
class QnWorkbench;
class QSortFilterProxyModel;
class QnResourceTreeSortProxyModel;

namespace Ui {
    class QnResourceTreeWidget;
}

class QnResourceTreeWidget : public QWidget {
    Q_OBJECT
    
    typedef QWidget base_type;

public:
    explicit QnResourceTreeWidget(QWidget *parent = 0);
    ~QnResourceTreeWidget();
    
    void setModel(QAbstractItemModel *model);

    QItemSelectionModel *selectionModel();

    void setWorkbench(QnWorkbench *workbench);

    void edit();

    void expand(const QModelIndex &index);

    void expandAll();

    QPoint selectionPos() const;

    /**
     * @brief setCheckboxesVisible      Show/hide checkboxes against each row.
     * @param visible                   Target state of checkboxes.
     */
    void setCheckboxesVisible(bool visible = true);

    /**
     * @brief isCheckboxesVisible       This property holds whether checkboxes against each row are visible.
     * @return                          True if checkboxes are visible.
     */
    bool isCheckboxesVisible() const;

    //TODO: #gdm flags?
    // TODO: #gdm setter starts with set, e.g. setGraphicsTweaksEnabled. And add a getter.
    void enableGraphicsTweaks(bool enableTweaks = true);

    /**
     * @brief setFilterVisible          Show/hide resource tree filter widget.
     * @param visible                   Target state of filter.
     */
    void setFilterVisible(bool visible = true);

    /**
     * @brief isFilterVisible           This property holds whether filter widget is visible.
     * @return                          True if filter is visible.
     */
    bool isFilterVisible() const;

    /**
     * @brief setEditingEnabled         Enable or disable item editing: renaming by F2 and moving by drag'n'drop.
     * @param enabled                   Whether editing should be allowed.
     */
    void setEditingEnabled(bool enabled = true);

    /**
     * @brief isEditingEnabled          This property holds whether renaming by F2 and moving by drag'n'drop are enabled.
     * @return                          True if editing is allowed.
     */
    bool isEditingEnabled() const;
protected:
    bool eventFilter(QObject *obj, QEvent *event);
    void resizeEvent(QResizeEvent *event) override;

    void updateCheckboxesVisibility();
signals:
    void activated(const QnResourcePtr &resource);
    void viewportSizeChanged();

private slots:
    void at_treeView_enterPressed(const QModelIndex &index);
    void at_treeView_doubleClicked(const QModelIndex &index);

    void at_resourceProxyModel_rowsInserted(const QModelIndex &parent, int start, int end);
    void at_resourceProxyModel_rowsInserted(const QModelIndex &index);

    void updateColumnsSize();
    void updateFilter();
private:
    QScopedPointer<Ui::QnResourceTreeWidget> ui;

    QnResourceTreeItemDelegate *m_itemDelegate;

    QnResourceTreeSortProxyModel *m_resourceProxyModel;

    /**
     * @brief m_checkboxesVisible       This property holds whether checkboxes against each row are visible.
     */
    bool m_checkboxesVisible;

    /**
     * @brief m_editingEnabled          This property holds whether renaming by F2 and moving by drag'n'drop are enabled.
     */
    bool m_editingEnabled;
};

#endif // RESOURCE_TREE_WIDGET_H
