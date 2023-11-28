// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/data/ldap.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class LdapSettingsWidget: public QnAbstractPreferencesWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

    using base_type = QnAbstractPreferencesWidget;

public:
    enum class TestState
    {
        initial,
        connecting,
        ok,
        error,
    };
    Q_ENUM(TestState)

    enum class Sync
    {
        disabled = (int) nx::vms::api::LdapSyncMode::disabled,
        groupsOnly = (int) nx::vms::api::LdapSyncMode::groupsOnly,
        usersAndGroups = (int) nx::vms::api::LdapSyncMode::usersAndGroups,
    };
    Q_ENUM(Sync)

public:
    explicit LdapSettingsWidget(QWidget* parent = nullptr);
    virtual ~LdapSettingsWidget() override;

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;
    virtual bool hasChanges() const override;
    virtual void discardChanges() override;
    virtual void resetWarnings() override;
    virtual bool isNetworkRequestRunning() const override;

    Q_INVOKABLE void testConnection(
        const QString& url,
        const QString& adminDn,
        const QString& password,
        bool startTls,
        bool ignoreCertErrors);

    Q_INVOKABLE void testOnline(
        const QString& url,
        const QString& adminDn,
        const QString& password,
        bool startTls,
        bool ignoreCertErrors);

    Q_INVOKABLE void cancelCurrentRequest();

    Q_INVOKABLE bool requestLdapReset();

    Q_INVOKABLE void requestSync();

protected:
    virtual void showEvent(QShowEvent* event) override;
    virtual void hideEvent(QHideEvent* event) override;

private:
    void checkOnlineAndSyncStatus();
    void showError(const QString& errorMessage);

signals:
    void stateChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
