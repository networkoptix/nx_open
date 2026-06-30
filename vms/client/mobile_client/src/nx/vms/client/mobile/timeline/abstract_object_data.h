// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/resource_fwd.h>
#include <recording/time_period_list.h>

Q_MOC_INCLUDE("core/resource/resource.h")

namespace nx::vms::client::mobile {
namespace timeline {

/**
 * Single object (bookmark, motion or analytics track) data, displayed in the object card (sheet).
 */
class AbstractObjectData: public QObject
{
    Q_OBJECT
    Q_PROPERTY(nx::Uuid id READ id NOTIFY changed)
    Q_PROPERTY(qint64 startTimeMs READ startTimeMs NOTIFY changed)
    Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString description READ description NOTIFY changed)
    Q_PROPERTY(QString imagePath READ imagePath NOTIFY changed)
    Q_PROPERTY(QVariant tags READ tags NOTIFY changed)
    Q_PROPERTY(QVariant attributes READ attributes NOTIFY changed)
    Q_PROPERTY(QnResource* resource READ getResource CONSTANT)

public:
    using QObject::QObject;
    virtual ~AbstractObjectData() = default;

    virtual nx::Uuid id() const = 0;
    virtual qint64 startTimeMs() const = 0;
    virtual qint64 durationMs() const = 0;
    virtual QString title() const = 0;
    virtual QString description() const = 0;
    virtual QString imagePath() const = 0;
    virtual QVariant tags() const = 0;
    virtual QVariant attributes() const = 0;
    virtual QnResourcePtr resource() const = 0;

    virtual common::CameraBookmark convertToBookmark() const;

    QnResource* getResource() const;

    static void registerQmlType();

public:
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

signals:
    void changed();
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
    QList<std::shared_ptr<AbstractObjectData>> perObjectData;

    QList<AbstractObjectData*> getPerObjectData() const;

    Q_GADGET
    Q_PROPERTY(QString caption MEMBER caption CONSTANT)
    Q_PROPERTY(QString description MEMBER description CONSTANT)
    Q_PROPERTY(QStringList iconPaths MEMBER iconPaths CONSTANT)
    Q_PROPERTY(QStringList imagePaths MEMBER imagePaths CONSTANT)
    Q_PROPERTY(qint64 positionMs MEMBER positionMs CONSTANT)
    Q_PROPERTY(qint64 durationMs MEMBER durationMs CONSTANT)
    Q_PROPERTY(int count MEMBER count CONSTANT)
    Q_PROPERTY(QList<AbstractObjectData*> perObjectData READ getPerObjectData CONSTANT)
};

} // namespace timeline
} // namespace nx::vms::client::mobile
