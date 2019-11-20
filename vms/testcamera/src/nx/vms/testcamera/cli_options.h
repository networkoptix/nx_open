#pragma once

#include <QtCore/QList>
#include <QtCore/QStringList>
#include <nx/utils/log/log_level.h>

namespace nx::vms::testcamera {

struct CliOptions
{
    struct CameraSet
    {
        int offlineFreq = 0;
        int count = 1;
        QStringList primaryFileNames; /**< Must not be empty. */
        QStringList secondaryFileNames; /**< Can be empty: primaryFileNames are re-used. */
    };

    bool showHelp = false;
    bool cameraForFile = false;
    bool includePts = false;
    bool noSecondary = false;
    int fpsPrimary = -1;
    int fpsSecondary = -1;
    bool unloopPts = false;
    int64_t shiftPtsPrimaryPeriodUs = -1;
    int64_t shiftPtsSecondaryPeriodUs = -1;
    QStringList localInterfaces;
    QList<CameraSet> cameraSets;
    nx::utils::log::Level logLevel = utils::log::Level::info;
};

bool parseCliOptions(int argc, const char* const argv[], CliOptions* options);

} // namespace nx::vms::testcamera
