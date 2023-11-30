// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <future>

#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/software_version.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/common/utils/command_action.h>
#include <nx/vms/client/desktop/window_context_aware.h>

namespace nx::vms::client::desktop {

struct UpdateContents;
class ServerUpdateTool;

class WorkbenchUpdateWatcher:
    public QObject,
    public WindowContextAware
{
    Q_OBJECT

public:
    WorkbenchUpdateWatcher(WindowContext* windowContext, QObject* parent = nullptr);
    virtual ~WorkbenchUpdateWatcher() override;

    /** Get cached update information. */
    const UpdateContents& getUpdateContents() const;
    ServerUpdateTool* getServerUpdateTool();
    std::future<UpdateContents> takeUpdateCheck();

private:
    void notifyUserAboutWorkbenchUpdate(
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
    /** Timer for checking state of current update check. Spins once per second. */
    QTimer m_updateStateTimer;
    /** Long timer for checking another update. Spins once per hour*/
    QTimer m_checkLatestUpdateTimer;
    nx::utils::SoftwareVersion m_notifiedVersion;

    CommandActionPtr m_updateAction;
    CommandActionPtr m_skipAction;
    QnUuid updateNotificationId;

    struct Private;
    nx::utils::ImplPtr<Private> m_private;
};

} // namespace nx::vms::client::desktop
