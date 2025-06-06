// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <set>

#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/common/dialogs/qml_dialog_with_state.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/client/desktop/window_context_aware.h>
#include <ui/workbench/workbench_state_manager.h>

#include "../models/members_model.h"

namespace nx::vms::client::desktop {

struct GroupSettingsDialogState
{
    Q_GADGET

    // Subject ID should go first for correct AccessSubjectEditingContext initialization.
    Q_PROPERTY(nx::Uuid groupId MEMBER groupId)

    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(bool nameEditable MEMBER nameEditable)
    Q_PROPERTY(QString description MEMBER description)
    Q_PROPERTY(bool descriptionEditable MEMBER descriptionEditable)
    Q_PROPERTY(bool isLdap MEMBER isLdap)
    Q_PROPERTY(bool isPredefined MEMBER isPredefined)
    Q_PROPERTY(QList<nx::vms::client::desktop::MembersModelGroup> parentGroups
        READ getParentGroups
        WRITE setParentGroups)
    Q_PROPERTY(bool parentGroupsEditable MEMBER parentGroupsEditable)
    Q_PROPERTY(QList<nx::Uuid> groups READ getGroups WRITE setGroups)
    Q_PROPERTY(MembersListWrapper users MEMBER users)
    Q_PROPERTY(bool membersEditable MEMBER membersEditable)
    Q_PROPERTY(nx::vms::api::GlobalPermissions globalPermissions MEMBER globalPermissions)
    Q_PROPERTY(nx::core::access::ResourceAccessMap sharedResources MEMBER sharedResources)
    Q_PROPERTY(bool permissionsEditable MEMBER permissionsEditable)
    Q_PROPERTY(bool nameIsUnique MEMBER nameIsUnique)

public:
    bool operator==(const GroupSettingsDialogState& other) const = default;

    nx::Uuid groupId;

    QString name;
    bool nameEditable = true;
    QString description;
    bool descriptionEditable = true;

    bool isLdap = false;
    bool isPredefined = false;

    std::set<MembersModelGroup> parentGroups;

    QList<MembersModelGroup> getParentGroups() const
    {
        return {parentGroups.begin(), parentGroups.end()};
    }

    void setParentGroups(const QList<MembersModelGroup>& v)
    {
        parentGroups = {v.begin(), v.end()};
    }

    bool parentGroupsEditable = true;
    std::set<nx::Uuid> groups;
    QList<nx::Uuid> getGroups() const { return {groups.begin(), groups.end()}; }
    void setGroups(const QList<nx::Uuid>& v) { groups = {v.begin(), v.end()}; }

    MembersListWrapper users;

    bool membersEditable = true;

    nx::vms::api::GlobalPermissions globalPermissions;
    nx::core::access::ResourceAccessMap sharedResources;
    bool permissionsEditable = true;
    bool nameIsUnique = true;
};

class NX_VMS_CLIENT_DESKTOP_API GroupSettingsDialog:
    public QmlDialogWithState<GroupSettingsDialogState, nx::Uuid>,
    SystemContextAware,
    WindowContextAware
{
    using base_type = QmlDialogWithState<GroupSettingsDialogState, nx::Uuid>;

    Q_OBJECT

public:
    enum DialogType
    {
        createGroup,
        editGroup
    };

public:
    GroupSettingsDialog(
        DialogType dialogType,
        SystemContext* systemContext,
        WindowContext* windowContext,
        QWidget* parent = nullptr);

    ~GroupSettingsDialog();

    bool setGroup(const nx::Uuid& groupId);

    Q_INVOKABLE QString validateName(const QString& text);
    Q_INVOKABLE bool isOkClicked() const { return acceptOnSuccess(); }
    Q_INVOKABLE QString infoText() const;
    Q_INVOKABLE QString toolTipText() const;

    static void removeGroups(
        WindowContext* windowContext,
        const QSet<nx::Uuid>& idsToRemove,
        nx::MoveOnlyFunc<void(bool, const QString&)> callback = {});

public slots:
    void onDeleteRequested();
    void onAddGroupRequested();

protected:
    virtual GroupSettingsDialogState createState(const nx::Uuid& groupId) override;
    virtual void saveState(const GroupSettingsDialogState& state) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};


} // namespace nx::vms::client::desktop
