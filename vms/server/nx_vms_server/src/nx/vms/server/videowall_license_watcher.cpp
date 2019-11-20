#include "videowall_license_watcher.h"

#include <chrono>

#include "ec2_connection.h"

#include <utils/common/synctime.h>
#include <utils/license_usage_helper.h>

#include <common/common_module.h>

#include <media_server/settings.h>
#include <media_server/media_server_module.h>

using namespace nx::vms::server;
using namespace std::chrono;

namespace {

constexpr milliseconds kLicenseCheckInterval = 1min;
constexpr int kLicenseErrorCounterLimit = 5;

} // namespace

VideoWallLicenseWatcher::VideoWallLicenseWatcher(QnMediaServerModule* serverModule):
    base_type(serverModule)
{
    m_videoWallStopTime = seconds(serverModule->settings().forceStopVideoWallTime());

    connect(&m_licenseTimer, &QTimer::timeout, this, &VideoWallLicenseWatcher::at_checkLicenses);
    m_licenseTimer.setInterval(kLicenseCheckInterval);
}

VideoWallLicenseWatcher::~VideoWallLicenseWatcher()
{
    stop();
}

void VideoWallLicenseWatcher::start()
{
    m_licenseTimer.start();
}

void VideoWallLicenseWatcher::stop()
{
    m_licenseTimer.stop();
}

void VideoWallLicenseWatcher::updateRuntimeInfoAfterVideoWallLicenseOverflowTransaction(
    qint64 expirationDate)
{
    QnPeerRuntimeInfo localInfo = runtimeInfoManager()->localInfo();
    if (localInfo.data.prematureVideoWallLicenseExpirationDate != expirationDate)
    {
        localInfo.data.prematureVideoWallLicenseExpirationDate = expirationDate;
        runtimeInfoManager()->updateLocalItem(localInfo);
    }
}

void VideoWallLicenseWatcher::syncLicenseOverflowTime(qint64 licenseOverflowTime)
{
    const bool marked = licenseOverflowTime != 0;
    auto connection = serverModule()->commonModule()->ec2Connection();
    auto miscManager = connection->getMiscManager(Qn::kSystemAccess);

    const auto errCode =
        miscManager->markVideoWallLicenseOverflowSync(marked, licenseOverflowTime);

    if (errCode == ec2::ErrorCode::ok)
        updateRuntimeInfoAfterVideoWallLicenseOverflowTransaction(licenseOverflowTime);
}

void VideoWallLicenseWatcher::at_checkLicenses()
{
    QnVideoWallLicenseUsageHelper helper(serverModule()->commonModule());

    if (!helper.isValid())
    {
        if (++m_tooManyVideoWallsCounter < kLicenseErrorCounterLimit)
            return; // Do not report license problem immediately.

        const qint64 licenseOverflowTime =
            runtimeInfoManager()->localInfo().data.prematureVideoWallLicenseExpirationDate;

        if (licenseOverflowTime == 0)
        {
            const auto overflowMs = duration_cast<milliseconds>(m_videoWallStopTime).count();
            const auto expirationDate = qnSyncTime->currentMSecsSinceEpoch() + overflowMs;

            syncLicenseOverflowTime(expirationDate);
        }
    }
    else
    {
        const qint64 licenseOverflowTime =
            runtimeInfoManager()->localInfo().data.prematureVideoWallLicenseExpirationDate;
        if (licenseOverflowTime)
            syncLicenseOverflowTime(0);

        m_tooManyVideoWallsCounter = 0;
    }
}
