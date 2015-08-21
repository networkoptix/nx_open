#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

class QnUserManagementWidget;
class QnUserListModel;
class QnSortedUserListModel;
class QnCheckBoxedHeaderView;

class QnUserManagementWidgetPrivate : public QObject, public QnWorkbenchContextAware {
    Q_OBJECT

    Q_DECLARE_PUBLIC(QnUserManagementWidget)
public:
    QnUserManagementWidgetPrivate(QnUserManagementWidget *parent);

    void setupUi();
    void updateFromSettings();
private:
    void at_usersTable_activated(const QModelIndex &index);
    void at_usersTable_clicked(const QModelIndex &index);
    void at_headerCheckStateChanged(Qt::CheckState state);
    
    Q_SLOT void at_mergeLdapUsersAsync_finished(int status, int handle, const QString &errorString);
    
    void createUser();
    void fetchUsers();
    void openLdapSettings();
    void updateSelection();

    void clearSelection();
    void setSelectedEnabled(bool enabled);
    void enableSelected();
    void disableSelected();
    void deleteSelected();

    QnUserResourceList visibleUsers() const;
    QnUserResourceList visibleSelectedUsers() const;
private:
    QnUserManagementWidget *q_ptr;
    QnUserListModel *m_usersModel;
    QnSortedUserListModel *m_sortModel;
    QnCheckBoxedHeaderView* m_header;
};
