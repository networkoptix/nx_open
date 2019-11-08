#include "videowall_license_watcher.h"

#include <chrono>

#include "ec2_connection.h"

#include "utils/common/synctime.h"

#include <utils/license_usage_helper.h>

#include <common/common_module.h>

using namespace nx::vms::server;
using namespace std::chrono;

namespace {

constexpr auto kLicenseCheckInterval = 10s;
constexpr int kLicenseErrorCounterLimit = 1;

} // namespace

VideoWallLicenseWatcher::VideoWallLicenseWatcher(QnCommonModule* commonModule):
    base_type(commonModule)
{
    connect(&m_licenseTimer, &QTimer::timeout, this, &VideoWallLicenseWatcher::at_checkLicenses);
    m_licenseTimer.setInterval(duration_cast<milliseconds>(kLicenseCheckInterval));
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
    qint64 prematureVideoWallLicenseExperationDate)
{
    QnPeerRuntimeInfo localInfo = runtimeInfoManager()->localInfo();
    if (localInfo.data.prematureVideoWallLicenseExperationDate != prematureVideoWallLicenseExperationDate)
    {
        localInfo.data.prematureVideoWallLicenseExperationDate = prematureVideoWallLicenseExperationDate;
        runtimeInfoManager()->updateLocalItem(localInfo);
    }
}

void VideoWallLicenseWatcher::at_checkLicenses()
{
    QnVideoWallLicenseUsageHelper helper(commonModule());

    if (!helper.isValid())
    {
        if (++m_tooManyVideoWallsCounter < kLicenseErrorCounterLimit)
            return; // Do not report license problem immediately.

        qint64 licenseOverflowTime = runtimeInfoManager()->localInfo().data.prematureVideoWallLicenseExperationDate;
        if (licenseOverflowTime == 0)
        {
            licenseOverflowTime = qnSyncTime->currentMSecsSinceEpoch();
            auto errCode = commonModule()->ec2Connection()->getMiscManager(Qn::kSystemAccess)->markVideoWallLicenseOverflowSync(true, licenseOverflowTime);
            if (errCode == ec2::ErrorCode::ok)
                updateRuntimeInfoAfterVideoWallLicenseOverflowTransaction(licenseOverflowTime);
        }
    }
    else
    {
        qint64 licenseOverflowTime = runtimeInfoManager()->localInfo().data.prematureVideoWallLicenseExperationDate;
        if (licenseOverflowTime)
        {
            auto errorCode = commonModule()->ec2Connection()->getMiscManager(Qn::kSystemAccess)->markVideoWallLicenseOverflowSync(false, 0);
            if (errorCode == ec2::ErrorCode::ok)
                updateRuntimeInfoAfterVideoWallLicenseOverflowTransaction(0);
        }

        m_tooManyVideoWallsCounter = 0;
    }
}
