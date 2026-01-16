// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QFuture>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <core/resource/resource_fwd.h>
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

    /** Asynchronous model data loading method to implement in descendants. */
    virtual QFuture<MultiObjectData> load(const QnTimePeriod& period,
        std::chrono::milliseconds minimumStackDuration) = 0;

    // Helper functions for descendants.

    /** Returns a cloud system id for a cross-system camera or an empty string otherwise. */
    static QString crossSystemId(const QnResourcePtr& resource);

    /**
     * Returns the name of the internal parameter for an image URL for `RemoteAsyncImageProvider`
     * to pass the system id for a cross-system camera thumbnail.
     */
    static QString systemIdParameterName();

    /**
     * Builds the path plus query part of a camera thumbnail URL for `RemoteAsyncImageProvider`.
     */
    static QString makeImageRequest(const QnResourcePtr& resource, qint64 timestampMs,
        int resolution, const QList<std::pair<QString, QString>>& extraParams = {});

    static constexpr int kLowImageResolution = 320;
    static constexpr int kHighImageResolution = 800;

    static constexpr int kMaxPreviewImageCount = 4;
};

} // namespace timeline
} // namespace nx::vms::client::mobile
