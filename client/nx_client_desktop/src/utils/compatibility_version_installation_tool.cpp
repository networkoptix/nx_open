#include "compatibility_version_installation_tool.h"

#include <QtCore/QTimer>

#include <api/applauncher_api.h>
#include <utils/applauncher_utils.h>
#include <utils/common/delayed.h>

using namespace applauncher;

namespace {

constexpr int kTimerIntervalMs = 500;

} // namespace

QnCompatibilityVersionInstallationTool::QnCompatibilityVersionInstallationTool(
    const nx::utils::SoftwareVersion& version)
    :
    m_version(version)
{
}

QnCompatibilityVersionInstallationTool::~QnCompatibilityVersionInstallationTool()
{
}

void QnCompatibilityVersionInstallationTool::run()
{
    const auto result = startInstallation(m_version, &m_installationId);
    if (result != api::ResultType::ok)
    {
        setStatus(Failed);
        return;
    }

    setStatus(Installing);

    QScopedPointer<QTimer> timer(new QTimer());
    connect(timer.data(), &QTimer::timeout,
        this, &QnCompatibilityVersionInstallationTool::at_timer_timeout);

    timer->start(kTimerIntervalMs);
    exec();
}

void QnCompatibilityVersionInstallationTool::cancel()
{
    executeLater([this]() { cancelInternal(); }, this);
}

void QnCompatibilityVersionInstallationTool::cancelInternal()
{
    if (m_status != Installing)
        return;

    const auto result = applauncher::cancelInstallation(m_installationId);
    setStatus(result == api::ResultType::ok
        ? Canceling
        : CancelFailed);
}

void QnCompatibilityVersionInstallationTool::setStatus(InstallationStatus status)
{
    if (m_status == status)
        return;

    m_status = status;
    emit statusChanged(status);
}

void QnCompatibilityVersionInstallationTool::finish(InstallationStatus status)
{
    setStatus(status);
    base_type::pleaseStop();
    quit();
}

void QnCompatibilityVersionInstallationTool::at_timer_timeout()
{
    if (m_status != Installing && m_status != Canceling)
        return;

    auto status = api::InstallationStatus::unknown;
    float installationProgress = 0;
    const auto result = getInstallationStatus(m_installationId, &status, &installationProgress);

    if (result != api::ResultType::ok)
    {
        finish(Failed);
        return;
    }

    switch (status)
    {
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

