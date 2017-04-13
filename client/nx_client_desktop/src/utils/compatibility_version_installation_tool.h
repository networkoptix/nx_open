#ifndef QNCOMPATIBILITYVERSIONINSTALLATIONTOOL_H
#define QNCOMPATIBILITYVERSIONINSTALLATIONTOOL_H

#include <QtCore/QObject>
#include <utils/common/long_runnable.h>
#include <utils/common/software_version.h>

class QnCompatibilityVersionInstallationTool : public QnLongRunnable {
    Q_OBJECT

    typedef QnCompatibilityVersionInstallationTool base_type;
public:
    enum InstallationStatus {
        Installing,
        Canceling,
        Success,
        Failed,
        Canceled,
        CancelFailed
    };

    explicit QnCompatibilityVersionInstallationTool(const QnSoftwareVersion &version);
    ~QnCompatibilityVersionInstallationTool();

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
    unsigned int m_installationId;
    InstallationStatus m_status;
    QnSoftwareVersion m_version;
};

#endif // QNCOMPATIBILITYVERSIONINSTALLATIONTOOL_H
