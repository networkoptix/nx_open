#pragma once

#include <QtCore/QString>

#include <nx/vms/server/nvr/types.h>

namespace nx::vms::server::nvr::hanwha {

struct PowerConsumptionLimits
{
    double lowerLimitWatts = 0.0;
    double upperLimitWatts = 0.0;
};

static const QString kWrn410("WRN-410");
static const QString kWrn810("WRN-810");
static const QString kWrn1610("WRN-1610");

PowerConsumptionLimits getPowerConsumptionLimit(const DeviceInfo& deviceInfo);

QString getModelByBoardId(int boardId);

} // namespace nx::vms::server::nvr::hanwha
