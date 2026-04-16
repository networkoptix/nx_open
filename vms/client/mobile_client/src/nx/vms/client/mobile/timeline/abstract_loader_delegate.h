// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QFuture>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <recording/time_period.h>

#include "abstract_object_data.h"

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

    /** Asynchronous model data loading method to implement in descendants. */
    virtual QFuture<MultiObjectData> load(const QnTimePeriod& period,
        std::chrono::milliseconds minimumStackDuration) = 0;
};

} // namespace timeline
} // namespace nx::vms::client::mobile
