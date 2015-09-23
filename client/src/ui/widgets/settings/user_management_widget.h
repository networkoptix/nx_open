#pragma once

#include <QtWidgets/QWidget>

#include <client/client_color_types.h>

#include <core/resource/resource_fwd.h>

#include <ui/widgets/settings/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

namespace Ui {
    class QnUserManagementWidget;
}

class QnUserListModel;
class QnSortedUserListModel;
class QnCheckBoxedHeaderView;

class QnUserManagementWidget : public Connective<QnAbstractPreferencesWidget>, public QnWorkbenchContextAware {
    Q_OBJECT
    Q_PROPERTY(QnUserManagementColors colors READ colors WRITE setColors)
    typedef Connective<QnAbstractPreferencesWidget> base_type;

public:
    explicit QnUserManagementWidget(QWidget *parent = 0);
    ~QnUserManagementWidget();

    virtual void updateFromSettings() override;

    const QnUserManagementColors colors() const;
    void setColors(const QnUserManagementColors &colors);
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
    QScopedPointer<Ui::QnUserManagementWidget> ui;
    QnUserListModel *m_usersModel;
    QnSortedUserListModel *m_sortModel;
    QnCheckBoxedHeaderView* m_header;
    QnUserManagementColors m_colors;
};
