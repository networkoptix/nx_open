#pragma once

#include <QtWidgets/QWidget>

class QnUserManagementWidget;
class QnUserListModel;
class QnSortedUserListModel;
class QnCheckBoxedHeaderView;

class QnUserManagementWidgetPrivate : public QObject {
    Q_OBJECT

    Q_DECLARE_PUBLIC(QnUserManagementWidget)
public:
    QnUserManagementWidgetPrivate(QnUserManagementWidget *parent);

    QnSortedUserListModel* sortModel() const;
    QHeaderView* header() const;
private:
    void at_usersTable_activated(const QModelIndex &index);
    void at_usersTable_clicked(const QModelIndex &index);
    void at_headerCheckStateChanged(Qt::CheckState state);
    
    Q_SLOT void at_mergeLdapUsersAsync_finished(int status, int handle, const QString &errorString);
    
    void fetchUsers();
    void openLdapSettings();
    void updateHeaderState();
private:
    QnUserManagementWidget *q_ptr;
    QnUserListModel *m_usersModel;
    QnSortedUserListModel *m_sortModel;
    QnCheckBoxedHeaderView* m_header;
};
