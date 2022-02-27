// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <ui/workbench/workbench_context_aware.h>
#include <nx/utils/software_version.h>
#include <nx/utils/url.h>
#include <future>

class QTimer;

namespace nx::vms::client::desktop {

struct UpdateContents;
class ServerUpdateTool;

class WorkbenchUpdateWatcher: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    WorkbenchUpdateWatcher(QObject* parent = nullptr);
    virtual ~WorkbenchUpdateWatcher() override;

    /** Get cached update information. */
    const UpdateContents& getUpdateContents() const;
    ServerUpdateTool* getServerUpdateTool();
    std::future<UpdateContents> takeUpdateCheck();

private:
    void showUpdateNotification(
        const nx::utils::SoftwareVersion& targetVersion,
        const nx::utils::Url& releaseNotesUrl,
        const QString& description);
    void atCheckerUpdateAvailable(const UpdateContents& info);
    /** Handler for starting update check. It is requested once an hour. */
    void atStartCheckUpdate();
    /** Handler for checking current request state. It is checked frequently. */
    void atUpdateCurrentState();
    void syncState();

private:
    /** We should not check for updates if this option is disabled in global settings. */
    bool m_autoChecksEnabled = false;
    /** We should not run update checks if we are logged out. */
    bool m_userLoggedIn = false;
    /** Timer for checking state of current update check. Spins once per second. */
    QTimer m_updateStateTimer;
    /** Long timer for checking another update. Spins once per hour*/
    QTimer m_checkLatestUpdateTimer;
    nx::utils::SoftwareVersion m_notifiedVersion;

    struct Private;
    std::unique_ptr<Private> m_private;
};

} // namespace nx::vms::client::desktop
