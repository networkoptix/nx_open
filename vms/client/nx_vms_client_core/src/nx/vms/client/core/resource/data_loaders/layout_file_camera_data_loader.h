// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>

#include <common/common_globals.h>
#include <nx/vms/client/core/common/data/motion_selection.h>

#include "abstract_camera_data_loader.h"

namespace nx::vms::client::core {

/**
 * Time period loader that can be used to load recorded time periods from exported layout
 * cameras.
 */

class QnLayoutFileCameraDataLoader: public AbstractCameraDataLoader
{
public:
    QnLayoutFileCameraDataLoader(
        const QnAviResourcePtr& resource,
        Qn::TimePeriodContent dataType,
        QObject* parent = nullptr);
    virtual ~QnLayoutFileCameraDataLoader();
    virtual void load(const QString& filter, const qint64 resolutionMs) override;

private:
    void sendDataDelayed();
    QnTimePeriodList loadMotion(
        const nx::vms::client::core::MotionSelection& motionSelection) const;

private:
    const QnAviResourcePtr m_aviResource;
};

} // namespace nx::vms::client::core
