#ifndef NAVIGATIONTREEWIDGET_H
#define NAVIGATIONTREEWIDGET_H

#include <QtGui/QWidget>

class QAbstractItemModel;
class QLineEdit;
class QSortFilterProxyModel;
class QToolButton;
class QTreeView;

class NavigationTreeWidget : public QWidget
{
    Q_OBJECT

public:
    NavigationTreeWidget(QWidget *parent = 0);
    ~NavigationTreeWidget();

Q_SIGNALS:
    void activated(uint resourceId);

private Q_SLOTS:
    void filterChanged(const QString &filter);
    void itemActivated(const QModelIndex &index);

private:
    QLineEdit *m_filterLineEdit;
    QToolButton *m_clearFilterButton;
    QAbstractItemModel *m_model;
    QSortFilterProxyModel *m_proxyModel;
    QTreeView *m_navigationTreeView;
};

#endif // NAVIGATIONTREEWIDGET_H
