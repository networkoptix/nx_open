#ifndef NAVIGATIONTREEWIDGET_H
#define NAVIGATIONTREEWIDGET_H

#include <QtGui/QWidget>

class QAbstractItemModel;
class QLineEdit;
class QSortFilterProxyModel;
class QTabWidget;
class QToolButton;
class QTreeView;

class NavigationTreeWidget : public QWidget
{
    Q_OBJECT

public:
    NavigationTreeWidget(QWidget *parent = 0);
    ~NavigationTreeWidget();

protected:
    void contextMenuEvent(QContextMenuEvent *event);

Q_SIGNALS:
    void activated(uint resourceId);

private Q_SLOTS:
    void filterChanged(const QString &filter);
    void itemActivated(const QModelIndex &index);
    void open();

private:
    QTabWidget *m_tabWidget;
    QToolButton *m_previousItemButton;
    QToolButton *m_nextItemButton;

    QLineEdit *m_filterLineEdit;
    QToolButton *m_clearFilterButton;

    QAbstractItemModel *m_resourcesModel;
    QTreeView *m_resourcesTreeView;
    QSortFilterProxyModel *m_searchProxyModel;
    QTreeView *m_searchTreeView;
};

#endif // NAVIGATIONTREEWIDGET_H
