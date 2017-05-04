#pragma once

#include <QtWidgets/QWidget>

#include <client/client_color_types.h>

#include <core/resource/resource_fwd.h>

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

namespace Ui
{
    class QnUserManagementWidget;
}

class QnUserListModel;
class QnSortedUserListModel;
class QnCheckBoxedHeaderView;

class QnUserManagementWidget : public Connective<QnAbstractPreferencesWidget>, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef Connective<QnAbstractPreferencesWidget> base_type;

public:
    explicit QnUserManagementWidget(QWidget* parent = nullptr);
    virtual ~QnUserManagementWidget();

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;
    virtual bool hasChanges() const override;

private:
    void at_usersTable_clicked(const QModelIndex& index);
    void at_headerCheckStateChanged(Qt::CheckState state);

    Q_SLOT void at_mergeLdapUsersAsync_finished(int status, int handle, const QString& errorString);

    void editRoles();
    void createUser();
    void fetchUsers();
    void openLdapSettings();

    void modelUpdated();
    void updateSelection();
    void updateLdapState();

    void clearSelection();
    bool enableUser(const QnUserResourcePtr& user, bool enabled);
    void setSelectedEnabled(bool enabled);
    void enableSelected();
    void disableSelected();
    void deleteSelected();

    QnUserResourceList visibleUsers() const;
    QnUserResourceList visibleSelectedUsers() const;
private:
    QScopedPointer<Ui::QnUserManagementWidget> ui;
    QnUserListModel* m_usersModel;
    QnSortedUserListModel* m_sortModel;
    QnCheckBoxedHeaderView* m_header;
};
