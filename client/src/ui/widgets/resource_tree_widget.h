#ifndef RESOURCE_TREE_WIDGET_H
#define RESOURCE_TREE_WIDGET_H

#include <QtCore/QAbstractItemModel>

#include <QtGui/QWidget>
#include <QtGui/QItemSelectionModel>

#include <core/resource/resource_fwd.h>

class QnResourceTreeItemDelegate;
class QnWorkbench;
class QSortFilterProxyModel;
class QnResourceSearchProxyModel;

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

    // TODO: #gdm Better use setCheckboxesVisible. And add a getter (bool isCheckboxesVisible() const).
    void setCheckboxesHidden(bool hidden = true);

    //TODO: #gdm flags?
    // TODO: #gdm setter starts with set, e.g. setGraphicsTweaksEnabled. And add a getter.
    void enableGraphicsTweaks(bool enableTweaks = true);

    /**
     * @brief setFilterVisible  Show/hide resource tree filter widget.
     * @param visible           Target state of filter.
     */
    void setFilterVisible(bool visible = true);

    /**
     * @brief isFilterVisible   This property holds whether filter widget is visible.
     * @return                  True if filter is visible.
     */
    bool isFilterVisible() const;
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

    QSortFilterProxyModel *m_resourceProxyModel;
    QnResourceSearchProxyModel *m_searchModel;

    bool m_checkboxesHidden;
};

#endif // RESOURCE_TREE_WIDGET_H
