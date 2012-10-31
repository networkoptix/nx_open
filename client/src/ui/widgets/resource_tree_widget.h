#ifndef RESOURCE_TREE_WIDGET_H
#define RESOURCE_TREE_WIDGET_H

#include <QtCore/QAbstractItemModel>

#include <QtGui/QWidget>
#include <QtGui/QItemSelectionModel>

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
private:
    QScopedPointer<Ui::QnResourceTreeWidget> ui;

    QnResourceTreeItemDelegate *m_itemDelegate;
};

#endif // RESOURCE_TREE_WIDGET_H
