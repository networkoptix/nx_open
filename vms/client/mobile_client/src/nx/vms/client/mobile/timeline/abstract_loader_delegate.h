// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QFuture>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <nx/utils/uuid.h>
#include <recording/time_period.h>

#include "objects_bucket.h"

namespace nx::vms::client::mobile {
namespace timeline {

/**
 * An abstract delegate that loads model data for one tile of `DataTimeline`.
 * See `ObjectsLoader` and `MultiObjectData`.
 */
class AbstractLoaderDelegate: public QObject
{
    Q_OBJECT

public:
    explicit AbstractLoaderDelegate(QObject* parent = nullptr): QObject(parent) {}

    virtual QFuture<MultiObjectData> load(const QnTimePeriod& period,
        std::chrono::milliseconds minimumStackDuration) = 0;

    static QString makeImageRequest(const nx::Uuid& cameraId, qint64 timestampMs, int resolution);
    static constexpr int kLowImageResolution = 320;
    static constexpr int kHighImageResolution = 800;

    static constexpr int kMaxPreviewImageCount = 4;
};

} // namespace timeline
} // namespace nx::vms::client::mobile
