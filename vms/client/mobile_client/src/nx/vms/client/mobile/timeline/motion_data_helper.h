// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

#include <recording/time_period.h>

#include "object_data.h"

namespace nx::vms::client::mobile {
namespace timeline {

class MotionDataHelper
{
    Q_DECLARE_TR_FUNCTIONS(nx::vms::client::mobile::timeline::MotionDataHelper)

public:
    static ObjectData makeObjectData(const QnTimePeriod& motion, const QnResourcePtr& resource);

    static qint64 previewTimeMs(const QnTimePeriod& motion);
    static QString iconPath();
};

} // namespace timeline
} // namespace nx::vms::client::mobile
