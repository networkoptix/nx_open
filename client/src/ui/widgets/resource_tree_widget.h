#ifndef RESOURCE_TREE_WIDGET_H
#define RESOURCE_TREE_WIDGET_H

#include <QtCore/QAbstractItemModel>

#include <QtGui/QWidget>
#include <QtGui/QItemSelectionModel>

#include <core/resource/resource_fwd.h>

class QnResourceTreeItemDelegate;
class QnWorkbench;

namespace Ui {
    class QnResourceTreeWidget;
}

class QnResourceTreeWidget : public QWidget
{
    Q_OBJECT
    
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

signals:
    void activated(const QnResourcePtr &resource);

private slots:
    void at_treeView_enterPressed(const QModelIndex &index);
    void at_treeView_doubleClicked(const QModelIndex &index);

private:
    QScopedPointer<Ui::QnResourceTreeWidget> ui;

    QnResourceTreeItemDelegate *m_itemDelegate;
};

#endif // RESOURCE_TREE_WIDGET_H
