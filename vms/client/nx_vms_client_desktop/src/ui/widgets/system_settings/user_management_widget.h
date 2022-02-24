// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>

namespace Ui { class QnUserManagementWidget; }
namespace nx::vms::client::desktop { class CheckableHeaderView; }

class QnUserListModel;
class QnSortedUserListModel;

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

    void filterDigestUsers();

private:
    void at_usersTable_clicked(const QModelIndex& index);
    void at_headerCheckStateChanged(Qt::CheckState state);

    Q_SLOT void at_mergeLdapUsersAsync_finished(int status, int handle, const QString& errorString);

    void editRoles();
    void createUser();
    void fetchUsers();
    void openLdapSettings();
    void forceSecureAuth();

    void modelUpdated();
    void updateSelection();
    void updateLdapState();

    bool enableUser(const QnUserResourcePtr& user, bool enabled);
    void setSelectedEnabled(bool enabled);
    void enableSelected();
    void disableSelected();
    void deleteSelected();

    QnUserResourceList visibleUsers() const;
    QnUserResourceList visibleSelectedUsers() const;

    bool canDisableDigest(const QnUserResourcePtr& user) const;

private:
    QScopedPointer<Ui::QnUserManagementWidget> ui;
    QnUserListModel* m_usersModel;
    QnSortedUserListModel* m_sortModel;
    nx::vms::client::desktop::CheckableHeaderView* m_header = nullptr;
    QAction* m_filterDigestAction = nullptr;
};
