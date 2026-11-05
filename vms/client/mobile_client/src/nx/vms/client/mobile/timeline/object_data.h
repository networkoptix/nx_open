// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>

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
    QString iconPath;
    QStringList tags;
    core::analytics::AttributeList attributes;
    bool shared = false;

    /** Source data: `common::CameraBookmark`, `analytics::db::ObjectTrack` or `QnTimePeriod`. */
    QVariant source;

    Q_GADGET
    Q_PROPERTY(nx::Uuid id MEMBER id CONSTANT)
    Q_PROPERTY(qint64 startTimeMs MEMBER startTimeMs CONSTANT)
    Q_PROPERTY(qint64 durationMs MEMBER durationMs CONSTANT)
    Q_PROPERTY(QString title MEMBER title CONSTANT)
    Q_PROPERTY(QString description MEMBER description CONSTANT)
    Q_PROPERTY(QString imagePath MEMBER imagePath CONSTANT)
    Q_PROPERTY(QString iconPath MEMBER iconPath CONSTANT)
    Q_PROPERTY(QStringList tags MEMBER tags CONSTANT)
    Q_PROPERTY(QVariant attributes READ getAttributes CONSTANT)
    Q_PROPERTY(bool shared MEMBER shared CONSTANT)

private:
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
    QList<ObjectData> perObjectData;

    Q_GADGET
    Q_PROPERTY(QString caption MEMBER caption CONSTANT)
    Q_PROPERTY(QString description MEMBER description CONSTANT)
    Q_PROPERTY(QStringList iconPaths MEMBER iconPaths CONSTANT)
    Q_PROPERTY(QStringList imagePaths MEMBER imagePaths CONSTANT)
    Q_PROPERTY(qint64 positionMs MEMBER positionMs CONSTANT)
    Q_PROPERTY(qint64 durationMs MEMBER durationMs CONSTANT)
    Q_PROPERTY(int count MEMBER count CONSTANT)
    Q_PROPERTY(QList<nx::vms::client::mobile::timeline::ObjectData> perObjectData MEMBER
            perObjectData CONSTANT)
};

/**
 * Returns the bookmark for `data`: the source bookmark itself, or a prefilled object-based
 * bookmark for other object types.
 */
common::CameraBookmark toBookmark(const ObjectData& data, const QnResourcePtr& resource);

/** Returns a cloud system id for a cross-system camera or an empty string otherwise. */
QString crossSystemId(const QnResourcePtr& resource);

/**
 * Returns the name of the internal parameter for an image URL for `RemoteAsyncImageProvider`
 * to pass the system id for a cross-system camera thumbnail.
 */
QString systemIdParameterName();

/** Builds the path plus query part of a camera thumbnail URL for `RemoteAsyncImageProvider`. */
QString makeImageRequest(const QnResourcePtr& resource,
    qint64 timestampMs,
    int resolution,
    const QList<std::pair<QString, QString>>& extraParams = {});

constexpr int kLowImageResolution = 320;
constexpr int kHighImageResolution = 800;

constexpr int kMaxPreviewImageCount = 4;

} // namespace timeline
} // namespace nx::vms::client::mobile
