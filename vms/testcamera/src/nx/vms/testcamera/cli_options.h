#pragma once

#include <vector>
#include <chrono>
#include <optional>

#include <QtCore/QStringList>

#include <nx/utils/log/log_level.h>

#include "video_layout.h"

namespace nx::vms::testcamera {

struct CliOptions
{
    struct CameraSet
    {
        int offlineFreq = 0;
        int count = 1;
        QStringList primaryFileNames; /**< Must not be empty. */
        QStringList secondaryFileNames; /**< Can be empty: primaryFileNames are re-used. */
        std::optional<VideoLayout> videoLayout;
    };

    using OptionalUs = std::optional<std::chrono::microseconds>;

    bool showHelp = false;
    nx::utils::log::Level logLevel = utils::log::Level::info;
    int maxFileSizeMegabytes = 100;
    bool cameraForFile = false;
    bool includePts = false;
    OptionalUs shiftPts;
    bool noSecondary = false;
    std::optional<int> fpsPrimary;
    std::optional<int> fpsSecondary;
    bool unloopPts = false;
    OptionalUs shiftPtsFromNow;
    OptionalUs shiftPtsPrimaryPeriod;
    OptionalUs shiftPtsSecondaryPeriod;
    QStringList localInterfaces;
    std::vector<CameraSet> cameraSets;
};

/**
 * Parses command-line arguments into the given data structure, and validates the values.
 *
 * If help is requested, prints it and exits the process with status 0.
 *
 * On error, prints the error message on stderr and returns false.
 */
bool parseCliOptions(int argc, const char* const argv[], CliOptions* options);

} // namespace nx::vms::testcamera
