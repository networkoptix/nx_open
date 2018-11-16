#pragma once
#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>
#include <nx/utils/software_version.h>
#include <nx/utils/url.h>

class QTimer;

namespace nx::update {
struct UpdateContents;
class UpdateCheckSignal;
} // namespace nx::update

class QnWorkbenchUpdateWatcher:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using UpdateContents = nx::update::UpdateContents;
    using UpdateCheckSignal = nx::update::UpdateCheckSignal;

public:
    QnWorkbenchUpdateWatcher(QObject* parent = nullptr);
    virtual ~QnWorkbenchUpdateWatcher() override;

    void start();
    void stop();

private:
    void showUpdateNotification(nx::utils::SoftwareVersion targetVersion,
                                nx::utils::Url releaseNotesUrl,
                                QString description);
    //void at_checker_updateAvailable(const QnUpdateInfo& info);
    void atCheckerUpdateAvailable(const UpdateContents& info);
    void atStartCheckUpdate();

private:
    QTimer* const m_timer;
    QPointer<UpdateCheckSignal> m_signal;
    nx::utils::SoftwareVersion m_notifiedVersion;
    std::future<UpdateContents> m_updateInfo;
};
