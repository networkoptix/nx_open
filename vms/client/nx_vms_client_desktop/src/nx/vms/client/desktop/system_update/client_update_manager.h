// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QDateTime>
#include <QtCore/QObject>
#include <QtCore/QUrl>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/software_version.h>
#include <nx/utils/uuid.h>

namespace nx::vms::common::update { struct Information; }

namespace nx::vms::client::desktop {

namespace workbench { class LocalNotificationsManager; }

/**
 * Client-only auto-update manager.
 */
class NX_VMS_CLIENT_DESKTOP_API ClientUpdateManager: public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool clientUpdateEnabled READ clientUpdateEnabled WRITE setClientUpdateEnabled
        NOTIFY clientUpdateEnabledChanged)
    Q_PROPERTY(nx::utils::SoftwareVersion pendingVersion READ pendingVersion
        NOTIFY pendingVersionChanged)
    Q_PROPERTY(QDateTime plannedUpdateDate READ plannedUpdateDate NOTIFY plannedUpdateDateChanged)
    Q_PROPERTY(bool installationNeeded READ installationNeeded NOTIFY installationNeededChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    ClientUpdateManager(QObject* parent = nullptr);
    virtual ~ClientUpdateManager() override;

    bool clientUpdateEnabled() const;
    void setClientUpdateEnabled(bool enabled);

    nx::utils::SoftwareVersion pendingVersion() const;
    QDateTime plannedUpdateDate() const;
    bool installationNeeded() const;

    QString errorMessage() const;

    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void speedUpCurrentUpdate();
    Q_INVOKABLE bool isPlannedUpdateDatePassed() const;
    Q_INVOKABLE QUrl releaseNotesUrl() const;

    struct UpdateDate
    {
        std::chrono::milliseconds date;
        std::chrono::milliseconds shift;
    };
    static UpdateDate calculateUpdateDate(
        std::chrono::milliseconds currentDateTime,
        const common::update::Information& updateInfo,
        const QnUuid& localSystemId,
        std::chrono::milliseconds minimumAllowedUpdateDate);

signals:
    void clientUpdateEnabledChanged();
    void pendingVersionChanged();
    void plannedUpdateDateChanged();
    void installationNeededChanged();
    void errorMessageChanged();

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
