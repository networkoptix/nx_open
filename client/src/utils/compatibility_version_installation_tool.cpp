#include "compatibility_version_installation_tool.h"

#include "api/applauncher_api.h"
#include "utils/applauncher_utils.h"

using namespace applauncher;

namespace {

    const int checkInterval = 500;

} // anonymous namespace

QnCompatibilityVersionInstallationTool::QnCompatibilityVersionInstallationTool(const QnSoftwareVersion &version) :
    QnLongRunnable(),
    m_status(Failed),
    m_version(version)
{
}

QnCompatibilityVersionInstallationTool::~QnCompatibilityVersionInstallationTool() {
}

void QnCompatibilityVersionInstallationTool::run() {
    api::ResultType::Value result = startInstallation(m_version, &m_installationId);

    if (result != api::ResultType::ok) {
        setStatus(Failed);
        return;
    }

    setStatus(Installing);

    QScopedPointer<QTimer> timer(new QTimer());
    connect(timer.data(), &QTimer::timeout, this, &QnCompatibilityVersionInstallationTool::at_timer_timeout);
    timer->start(checkInterval);

    exec();
}

void QnCompatibilityVersionInstallationTool::cancel() {
    QTimer::singleShot(0, this, SLOT(cancelInternal()));
}

void QnCompatibilityVersionInstallationTool::cancelInternal() {
    if (m_status != Installing)
        return;

    api::ResultType::Value result = applauncher::cancelInstallation(m_installationId);

    if (result != api::ResultType::ok) {
        setStatus(CancelFailed);
        return;
    }

    setStatus(Canceling);
}

void QnCompatibilityVersionInstallationTool::setStatus(QnCompatibilityVersionInstallationTool::InstallationStatus status) {
    if (m_status == status)
        return;

    m_status = status;
    emit statusChanged(status);
}

void QnCompatibilityVersionInstallationTool::finish(InstallationStatus status) {
    setStatus(status);
    base_type::pleaseStop();
    quit();
}

void QnCompatibilityVersionInstallationTool::at_timer_timeout() {
    if (m_status != Installing && m_status != Canceling)
        return;

    api::InstallationStatus::Value status = api::InstallationStatus::unknown;
    float installationProgress = 0;
    const api::ResultType::Value result = getInstallationStatus(m_installationId, &status, &installationProgress);

    if (result != api::ResultType::ok) {
        finish(Failed);
        return;
    }

    switch (status) {
    case api::InstallationStatus::inProgress:
    case api::InstallationStatus::cancelInProgress:
        emit progressChanged(static_cast<int>(installationProgress));
        break;
    case api::InstallationStatus::failed:
        finish(m_status == Installing ? Failed : CancelFailed);
        break;
    case api::InstallationStatus::success:
        emit progressChanged(100);
        finish(m_status == Installing ? Success : Canceled);
        break;
    case api::InstallationStatus::cancelled:
        finish(Canceled);
        break;
    default:
        break;
    }
}

