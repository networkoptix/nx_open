// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>
#include <recording/time_period_list.h>

namespace nx::vms::client::mobile {
namespace timeline {

/**
 * Single object (bookmark, motion or analytics track) data, displayed in the object card (sheet).
 */
struct ObjectData
{
    nx::Uuid id;
    qint64 startTimeMs{};
    qint64 durationMs{};
    QString title;
    QString description;
    QString imagePath;
    std::optional<QStringList> tags;
    std::optional<core::analytics::AttributeList> attributes;

    bool operator==(const ObjectData& other) const = default;
    bool operator!=(const ObjectData& other) const = default;

    Q_GADGET
    Q_PROPERTY(nx::Uuid id MEMBER id CONSTANT)
    Q_PROPERTY(qint64 startTimeMs MEMBER startTimeMs CONSTANT)
    Q_PROPERTY(qint64 durationMs MEMBER durationMs CONSTANT)
    Q_PROPERTY(QString title MEMBER title CONSTANT)
    Q_PROPERTY(QString description MEMBER description CONSTANT)
    Q_PROPERTY(QString imagePath MEMBER imagePath CONSTANT)
    Q_PROPERTY(QVariant tags READ getTags CONSTANT)
    Q_PROPERTY(QVariant attributes READ getAttributes CONSTANT)

private:
    QVariant getTags() const;
    QVariant getAttributes() const;
};

/**
 * Single or multiple object data, displayed in the object tile in the timeline.
 */
struct MultiObjectData
{
    QString caption;
    QString description;
    QStringList iconPaths;
    QStringList imagePaths;
    qint64 positionMs{};
    qint64 durationMs{};
    int count{};
    QnTimePeriodList objectChunks;
    QList<ObjectData> perObjectData;

    bool operator==(const MultiObjectData& other) const;
    bool operator!=(const MultiObjectData& other) const;

    Q_GADGET
    Q_PROPERTY(QString caption MEMBER caption CONSTANT)
    Q_PROPERTY(QString description MEMBER description CONSTANT)
    Q_PROPERTY(QStringList iconPaths MEMBER iconPaths CONSTANT)
    Q_PROPERTY(QStringList imagePaths MEMBER imagePaths CONSTANT)
    Q_PROPERTY(qint64 positionMs MEMBER positionMs CONSTANT)
    Q_PROPERTY(qint64 durationMs MEMBER durationMs CONSTANT)
    Q_PROPERTY(int count MEMBER count CONSTANT)
    Q_PROPERTY(QList<ObjectData> perObjectData MEMBER perObjectData CONSTANT)
};

/**
 * A temporal range in the timeline, possibly containing a single or multiple object data,
 * with its state. The duration is not stored as it's constant for all buckets.
 */
struct ObjectBucket
{
    enum State
    {
        Initial,
        Empty,
        Ready
    };
    Q_ENUM(State)

    qint64 startTimeMs{};
    State state = State::Initial;
    std::optional<MultiObjectData> data;
    bool isLoading = false;

    MultiObjectData getData() const;

    Q_GADGET
    Q_PROPERTY(qint64 startTimeMs MEMBER startTimeMs CONSTANT)
    Q_PROPERTY(MultiObjectData data READ getData)
    Q_PROPERTY(State state MEMBER state)
    Q_PROPERTY(bool isLoading MEMBER isLoading)
};

} // namespace timeline
} // namespace nx::vms::client::mobile
