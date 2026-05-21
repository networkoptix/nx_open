// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QSize>
#include <QtGui/QImage>

class QnAspectRatio;

namespace nx::vms::client::desktop {

/**
 * @brief The QnThreadedImageLoader class is intended for threaded processing of images.
 * It allows to read, resize and save images in a separate thread. Does not require explicit
 * memory deallocation.
 */
class ThreadedImageLoader: public QObject
{
    Q_OBJECT

public:
    /**
    * Loader flags.
    */
    enum Flag
    {
        /** If set, image will not be upscaled. */
        DownscaleOnly = 0x1,

        /** If set, image will be scaled to size to touch it from outside (default is touch inside). */
        TouchSizeFromOutside = 0x2,

        /** If set, image will be cropped to target aspectRatio. */
        CropToTargetAspectRatio = 0x4
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    explicit ThreadedImageLoader(QObject* parent = nullptr);
    virtual ~ThreadedImageLoader() override;

    /**
     * @brief setSize               Set target size of an image.
     * @param size                  Size in pixels
     */
    void setSize(const QSize& size);

    /**
     * Set target aspect ratio of an image. If CropToTargetAspectRatio flag is set, image will be
     * cropped to this aspect ratio, else target aspect ratio will be ignored.
     * @param aspectRatio Aspect ratio as width to height.
     */
    void setAspectRatio(const QnAspectRatio& aspectRatio);

    /**
     * @brief setTransformationMode Set transformation mode used in resize. Default value is Qt::SmoothTransformation.
     * @param mode                  Qt::TransformationMode
     */
    void setTransformationMode(const Qt::TransformationMode mode);

    /**
     * Set various loader flags. @see ThreadedImageLoaderFlags for details.
     * @param flags Default is DownscaleOnly | CropToTargetAspectRatio;
     */
    void setFlags(Flags flags);

    /**
     * Input image file. Have lower priority than setInput(QString)
     */
    void setInput(const QImage& input);

    /**
     * Path to an image. Have greater priority than setInput(QImage)
     * @param filename Full path as used in current OS.
     */
    void setInput(const QString& filename);

    /**
     * Path where output image should be saved. If set, no direct QImage output will be provided.
     * Allows to convert the image from one format to another.
     * @param filename Target filename.
     */
    void setOutput(const QString& filename);

    /**
     * Start the image processing in a separate thread.
     */
    void start();

signals:
    /**
     * Emitted when processing is finished and Output param is NOT SET. `output` is a null QImage
     * if the input could not be read or produced a null image.
     * @param output  Result image, null on failure.
     */
    void imageLoaded(const QImage& output);

    /**
     * Emitted when processing is finished and Output param is SET. `output` is an empty string if
     * the input could not be read or the file could not be written to disk.
     * @param output  Full path to the result file, empty on failure.
     */
    void imageSaved(const QString& output);

private:
    struct ThreadedImageLoaderWorker;
    friend struct ThreadedImageLoaderWorker; //< In order not to duplicate setters.

    struct Parameters;
    QScopedPointer<Parameters> parameters;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ThreadedImageLoader::Flags)

} // namespace nx::vms::client::desktop
