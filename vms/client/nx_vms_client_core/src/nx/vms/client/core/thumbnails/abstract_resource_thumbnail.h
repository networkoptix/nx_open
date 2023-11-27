// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtGui/QImage>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <utils/common/aspect_ratio.h>

Q_MOC_INCLUDE("core/resource/resource.h")

namespace nx::vms::client::core {

class AsyncImageResult;

/**
 * A base class of QML-ready resource thumbnails.
 *
 * Descendants can implement different image loading mechanisms for different resource types
 * and different needs, as well as image caching and sharing, periodic refresh and other stuff.
 *
 * When used in QML code a connection to the `imageChanged` signal must be made,
 * as an image can change with `url` property value not changing.
 */
class NX_VMS_CLIENT_CORE_API AbstractResourceThumbnail: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QnResource* resource READ rawResource WRITE setRawResource NOTIFY resourceChanged)
    Q_PROPERTY(int maximumSize READ maximumSize WRITE setMaximumSize NOTIFY maximumSizeChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(qreal aspectRatio READ aspectRatio NOTIFY aspectRatioChanged)
    Q_PROPERTY(int rotationQuadrants READ rotationQuadrants NOTIFY rotationChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(QImage image READ image NOTIFY imageChanged)
    Q_PROPERTY(QUrl url READ url NOTIFY imageChanged)
    Q_PROPERTY(int obsolescenceMinutes READ obsolescenceMinutes WRITE setObsolescenceMinutes
        NOTIFY obsolescenceMinutesChanged)
    Q_PROPERTY(bool obsolete READ isObsolete NOTIFY obsoleteChanged)
    Q_PROPERTY(bool isArmServer READ isArmServer NOTIFY isArmServerChanged)

    using base_type = QObject;

public:
    explicit AbstractResourceThumbnail(QObject* parent = nullptr);
    virtual ~AbstractResourceThumbnail() override;

    /** Thumbnail loading status. */
    enum class Status
    {
        empty, //< Loading has not been started.
        loading, //< Loading or reloading is in progress.
        ready, //< Loading has finished, thumbnail has been obtained.
        unavailable //< Loading has finished, no thumbnail is available.
    };
    Q_ENUM(Status)

    /**
     * Resource to load thumbnails for.
     * Make sure to clean the resource if it's removed from the pool, to release the shared pointer.
     *
     * These accessors are to be used from C++ code.
     */
    QnResourcePtr resource() const;
    void setResource(const QnResourcePtr& value);
    /**
     * These accessors are to be used from QML code (via "resource" property).
     */
    QnResource* rawResource() const;
    void setRawResource(QnResource* value);

    /** Maximum width or height for a thumbnail image. */
    int maximumSize() const;
    void setMaximumSize(int value);
    static constexpr int kUnlimitedSize = 0;

    /**
     * Whether this thumbnail is active. Default value is `true`.
     * Inactive thumbnails do not attempt to load or reload.
     */
    bool active() const;
    void setActive(bool value);

    /** Aspect ratio obtained from the resource. */
    qreal aspectRatio() const;

    /**
     * Rotation obtained from the resource, in steps of 90 degrees ccw.
     * Should be applied to the image.
     */
    int rotationQuadrants() const;

    /** Image loading status. */
    Status status() const;

    /**
     * Current image. Empty if no image has been loaded.
     * If thumbnail is being reloaded, previously loaded image is available.
     */
    QImage image() const;

    /**
     * URL to access the image from QML code.
     * Empty if no image has been loaded.
     * Always changes if the image changes.
     */
    QUrl url() const;

    /**
     * Whether a thumbnail image is obsolete.
     * Obsolescence is supported only for resources with UTC timestamps.
     */
    bool isObsolete() const;

    /**
     * The period after which a thumbnail is considered obsolete.
     * If zero, obsolescence is off. Default is zero.
     */
    int obsolescenceMinutes() const;
    void setObsolescenceMinutes(int value);

    /** Whether the resource is remote and is located on an ARM server. */
    bool isArmServer() const;

    /**
     * Reload the thumbnail.
     * If `forceRefresh` is true, cached image (if present) must be discarded.
     * Update is requested not immediately but put in the event queue.
     */
    Q_INVOKABLE void update(bool forceRefresh = false);

    static QnAspectRatio calculateAspectRatio(const QnResourcePtr& resource,
        const QnAspectRatio& defaultAspectRatio = QnAspectRatio(),
        bool* aspectRatioPredefined = nullptr);

    static void registerQmlType();

signals:
    void resourceChanged();
    void maximumSizeChanged();
    void activeChanged();
    void aspectRatioChanged();
    void rotationChanged();
    void statusChanged();
    void imageChanged();
    void obsoleteChanged();
    void obsolescenceMinutesChanged();
    void updated();
    void isArmServerChanged();

protected:
    /**
     * Primary image loading mechanism.
     * AsyncImageResult descendants can wrap asynchronous or synchronous image requests.
     * ImmediateImageResult can be used if an image is already available.
     */
    virtual std::unique_ptr<AsyncImageResult> getImageAsync(bool forceRefresh) const = 0;

    /**
     * If an image is updated externally (for example, in case of a common cache entry refresh),
     * this method should be called to update the thumbnail.
     */
    void setImage(const QImage& image);

    /** Clears current image, resets status to `empty` and cancels loading if it's in progress. */
    void reset();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

NX_VMS_CLIENT_CORE_API QString toString(AbstractResourceThumbnail::Status value);

} // namespace nx::vms::client::core
