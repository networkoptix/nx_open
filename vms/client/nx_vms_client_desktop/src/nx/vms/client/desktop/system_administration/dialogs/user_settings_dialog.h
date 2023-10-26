// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/api/types/access_rights_types.h>
#include <nx/vms/client/desktop/common/dialogs/qml_dialog_with_state.h>
#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/client/desktop/window_context_aware.h>
#include <ui/workbench/workbench_state_manager.h>

#include "../globals/user_settings_global.h"
#include "../models/members_model.h"

namespace nx::vms::client::desktop {

struct UserSettingsDialogState
{
    Q_GADGET

    // Subject ID should go first for correct AccessSubjectEditingContext initialization.
    Q_PROPERTY(QnUuid userId MEMBER userId)

    Q_PROPERTY(nx::vms::client::desktop::UserSettingsGlobal::UserType userType MEMBER userType)
    Q_PROPERTY(bool isSelf MEMBER isSelf)

    Q_PROPERTY(bool deleteAvailable MEMBER deleteAvailable)
    Q_PROPERTY(bool auditAvailable MEMBER auditAvailable)

    Q_PROPERTY(QString login MEMBER login)
    Q_PROPERTY(bool loginEditable MEMBER loginEditable)
    Q_PROPERTY(QString fullName MEMBER fullName)
    Q_PROPERTY(bool fullNameEditable MEMBER fullNameEditable)
    Q_PROPERTY(QString email MEMBER email)
    Q_PROPERTY(bool emailEditable MEMBER emailEditable)
    Q_PROPERTY(QString password MEMBER password)
    Q_PROPERTY(bool passwordEditable MEMBER passwordEditable)
    Q_PROPERTY(bool userEnabled MEMBER userEnabled)
    Q_PROPERTY(bool userEnabledEditable MEMBER userEnabledEditable)
    Q_PROPERTY(bool allowInsecure MEMBER allowInsecure)
    Q_PROPERTY(bool allowInsecureEditable MEMBER allowInsecureEditable)
    Q_PROPERTY(QList<nx::vms::client::desktop::MembersModelGroup> parentGroups
        READ getParentGroups
        WRITE setParentGroups)
    Q_PROPERTY(bool parentGroupsEditable MEMBER parentGroupsEditable)
    Q_PROPERTY(nx::core::access::ResourceAccessMap sharedResources MEMBER sharedResources)
    Q_PROPERTY(nx::vms::api::GlobalPermissions globalPermissions MEMBER globalPermissions)
    Q_PROPERTY(bool permissionsEditable MEMBER permissionsEditable)
    Q_PROPERTY(bool linkEditable MEMBER linkEditable)
    Q_PROPERTY(bool nameIsUnique MEMBER nameIsUnique)
    Q_PROPERTY(bool userIsNotRegisteredInCloud MEMBER userIsNotRegisteredInCloud)

public:
    bool operator==(const UserSettingsDialogState& other) const = default;

    UserSettingsGlobal::UserType userType = UserSettingsGlobal::LocalUser;
    bool isSelf = false;
    QnUuid userId;

    bool deleteAvailable = true;
    bool auditAvailable = true;

    QString login;
    bool loginEditable = true;
    QString fullName;
    bool fullNameEditable = true;
    QString email;
    bool emailEditable = true;
    QString password;
    bool passwordEditable = true;
    bool userEnabled = true;
    bool userEnabledEditable = true;
    bool allowInsecure = false;
    bool allowInsecureEditable = true;
    bool nameIsUnique = true;

    // Expose set of groups as QList for interoperability with QML and correct state comparison.
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

    nx::core::access::ResourceAccessMap sharedResources;
    nx::vms::api::GlobalPermissions globalPermissions;
    bool permissionsEditable = true;
    bool linkEditable = true;

    bool userIsNotRegisteredInCloud = false;
};

class NX_VMS_CLIENT_DESKTOP_API UserSettingsDialog:
    public QmlDialogWithState<UserSettingsDialogState, QnUserResourcePtr>,
    SystemContextAware,
    WindowContextAware
{
    using base_type = QmlDialogWithState<UserSettingsDialogState, QnUserResourcePtr>;
    Q_OBJECT

public:
    enum DialogType
    {
        CreateUser,
        EditUser
    };

    enum Tab
    {
        GeneralTab,
        GroupsTab,
        ResourcesTab,
        GlobalPermissionsTab,

        TabCount
    };

public:
    UserSettingsDialog(
        DialogType dialogType,
        SystemContext* systemContext,
        WindowContext* windowContext,
        QWidget* parent = nullptr);

    virtual ~UserSettingsDialog() override;

    bool setUser(const QnUserResourcePtr& user);
    void selectTab(Tab tab);

    Q_INVOKABLE QString validateLogin(const QString& login);
    Q_INVOKABLE QString validateEmail(const QString& email, bool forCloud = false);
    Q_INVOKABLE QString validateCurrentPassword(const QString& password);

    Q_INVOKABLE QString extractEmail(const QString& userInput);

    Q_INVOKABLE bool isConnectedToCloud() const;

    Q_INVOKABLE bool isOkClicked() const { return acceptOnSuccess(); }

    Q_INVOKABLE QDateTime newValidUntilDate() const;
    Q_INVOKABLE QString durationFormat(qint64 ms) const;

    Q_INVOKABLE QString warningForTemporaryUser(
        const QList<nx::vms::client::desktop::MembersModelGroup>& parentGroups,
        const nx::core::access::ResourceAccessMap& sharedResources,
        const nx::vms::api::GlobalPermissions permissions) const;

    Q_INVOKABLE void cancelRequest();

    /**
     * The time offset between time zone of a server and a client is not a constant value and
     * depends on the date on which this offset is calculated. This is due to conversion of clocks
     * to daylight saving time in some regions.
     */
    Q_INVOKABLE int displayOffset(qint64 msecsSinceEpoch);

public slots:
    void onDeleteRequested();
    void onAuditTrailRequested();
    void onAddGroupRequested();
    void onTerminateLink();
    void onResetLink(const QDateTime& validUntil, int revokeAccessAfterS, const QJSValue& callback);
    void onCopyLink();

protected:
    virtual UserSettingsDialogState createState(const QnUserResourcePtr& user) override;
    virtual void saveState(const UserSettingsDialogState& state) override;
    void refreshToken(const QString& password);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
