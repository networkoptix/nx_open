#ifndef QNCOMPATIBILITYVERSIONINSTALLATIONTOOL_H
#define QNCOMPATIBILITYVERSIONINSTALLATIONTOOL_H

#include <QtCore/QObject>

#include <nx/utils/software_version.h>
#include <nx/utils/thread/long_runnable.h>

class QnCompatibilityVersionInstallationTool: public QnLongRunnable
{
    Q_OBJECT
    using base_type = QnCompatibilityVersionInstallationTool;

public:
    enum InstallationStatus
    {
        Installing,
        Canceling,
        Success,
        Failed,
        Canceled,
        CancelFailed
    };

    explicit QnCompatibilityVersionInstallationTool(const nx::utils::SoftwareVersion& version);
    virtual ~QnCompatibilityVersionInstallationTool() override;

    virtual void run() override;

public slots:
    void cancel();

signals:
    void progressChanged(int progress);
    void statusChanged(int status);

private:
    void setStatus(InstallationStatus status);
    void finish(InstallationStatus status);

private slots:
    void cancelInternal();
    void at_timer_timeout();

private:
    unsigned int m_installationId = 0;
    InstallationStatus m_status = Failed;
    nx::utils::SoftwareVersion m_version;
};

#endif // QNCOMPATIBILITYVERSIONINSTALLATIONTOOL_H
