#pragma once
#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>
#include <nx/utils/software_version.h>
#include <nx/utils/url.h>

class QTimer;

namespace nx::update {
struct UpdateContents;
class UpdateCheckNotifier;
} // namespace nx::update

class QnWorkbenchUpdateWatcher:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using UpdateContents = nx::update::UpdateContents;
    using UpdateCheckNotifier = nx::update::UpdateCheckNotifier;

public:
    QnWorkbenchUpdateWatcher(QObject* parent = nullptr);
    virtual ~QnWorkbenchUpdateWatcher() override;

    void start();
    void stop();

private:
    void showUpdateNotification(
        const nx::utils::SoftwareVersion& targetVersion,
        const nx::utils::Url& releaseNotesUrl,
        const QString& description);
    void atCheckerUpdateAvailable(const UpdateContents& info);
    void atStartCheckUpdate();

private:
    QTimer* const m_timer;
    QPointer<UpdateCheckNotifier> m_signal;
    nx::utils::SoftwareVersion m_notifiedVersion;
    std::future<UpdateContents> m_updateInfo;
};
