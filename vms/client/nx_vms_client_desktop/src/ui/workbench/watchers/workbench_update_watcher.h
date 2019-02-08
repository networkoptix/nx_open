#pragma once
#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>
#include <nx/utils/software_version.h>
#include <nx/utils/url.h>

class QTimer;

namespace nx::update {
struct UpdateContents;
} // namespace nx::update

class QnWorkbenchUpdateWatcher:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using UpdateContents = nx::update::UpdateContents;

public:
    QnWorkbenchUpdateWatcher(QObject* parent = nullptr);
    virtual ~QnWorkbenchUpdateWatcher() override;

    void start();
    void stop();

    /** Get cached update information. */
    const UpdateContents& getUpdateContents() const;

private:
    void showUpdateNotification(
        const nx::utils::SoftwareVersion& targetVersion,
        const nx::utils::Url& releaseNotesUrl,
        const QString& description);
    void atCheckerUpdateAvailable(const UpdateContents& info);
    void atStartCheckUpdate();
    void syncState();

private:
    /** We should not check for updates if this option is disabled in global settings. */
    bool m_autoChecksEnabled = false;
    /** We should not run update checks if we are logged out. */
    bool m_userLoggedIn = false;
    QTimer* const m_timer;
    nx::utils::SoftwareVersion m_notifiedVersion;

    struct Private;
    std::unique_ptr<Private> m_private;
};
