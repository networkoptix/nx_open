#ifndef THREADED_IMAGE_LOADER_H
#define THREADED_IMAGE_LOADER_H

#include <QtCore/QObject>
#include <QtCore/QSize>
#include <QtCore/QThread>

#include <QtGui/QImage>

namespace Qn {
    /**
    * Loader flags.
    */
    enum ImageLoaderFlag {
        /** If set, image will not be upscaled. */
        DownscaleOnly = 0x1,

        /** If set, image will be scaled to size to touch it from outside (default is touch inside). */
        TouchSizeFromOutside = 0x2,

        /** If set, image will be cropped to target aspectRatio. */
        CropToTargetAspectRatio = 0x4
    };
    Q_DECLARE_FLAGS(ImageLoaderFlags, ImageLoaderFlag)

}

/**
 * @brief The QnThreadedImageLoaderPrivate class is private class. DO NOT USE it in your modules.
 */
class QnThreadedImageLoaderPrivate : public QObject
{
    Q_OBJECT
public:
    explicit QnThreadedImageLoaderPrivate();
    ~QnThreadedImageLoaderPrivate();
public slots:
    void setSize(const QSize &size);
    void setAspectRatio(const qreal aspectRatio);
    void setTransformationMode(const Qt::TransformationMode mode);
    void setFlags(const Qn::ImageLoaderFlags flags);

    void setInput(const QImage &input);
    void setInput(const QString &filename);
    void setOutput(const QString &filename);

    void start();
signals:
    void finished(const QImage &output);
    void finished(const QString &output);

private:
    QImage m_input;
    QSize m_size;
    qreal m_aspectRatio;
    Qt::TransformationMode m_transformationMode;
    QString m_inputFilename;
    QString m_outputFilename;
    Qn::ImageLoaderFlags m_flags;
};


/**
 * @brief The QnThreadedImageLoader class is intended for threaded processing of images.
 * It allows to read, resize and save images in a separate thread. Does not require explicit
 * memory deallocation.
 */
class QnThreadedImageLoader : public QObject
{
    Q_OBJECT
public:
    explicit QnThreadedImageLoader(QObject *parent = 0);
    ~QnThreadedImageLoader();

public slots:
    /**
     * @brief setSize               Set target size of an image.
     * @param size                  Size in pixels
     */
    void setSize(const QSize &size);

    /**
     * @brief setAspectRatio        Set target aspect ratio of an image. If CropToTargetAspectRatio flag is set, image
     *                              will be cropped to this aspect ratio, else target aspect ratio will be ignored.
     *                              If set to 0.0, original aspect ratio will be kept. Default is 0.0.
     * @param aspectRatio           Aspect ratio as width to height.
     */
    void setAspectRatio(const qreal aspectRatio);

    /**
     * @brief setTransformationMode Set transformation mode used in resize. Default value is Qt::SmoothTransformation.
     * @param mode                  Qt::TransformationMode
     */
    void setTransformationMode(const Qt::TransformationMode mode);


    /**
     * @brief setFlags              Set various loader flags. @see Qn::ImageLoaderFlags for details.
     * @param flags                 Set of Qn::ImageLoaderFlag values. Default is DownscaleOnly | CropToTargetAspectRatio;
     */
    void setFlags(const Qn::ImageLoaderFlags flags);

    /**
     * @brief setInput              Input image file. Have lower priority than setInput(QString)
     * @param input
     */
    void setInput(const QImage &input);

    /**
     * @brief setInput              Path to an image. Have greater priority than setInput(QImage)
     * @param filename              Full path as used in currect OS.
     */
    void setInput(const QString &filename);

    /**
     * @brief setOutput             Path where output image should be saved. If set, no direct QImage output
     *                              will be provided. Allows to convert the image from one format to another.
     * @param filename
     */
    void setOutput(const QString &filename);

    /**
     * @brief start                 Start the image processing.
     */
    void start();
signals:

    /**
     * @brief finished              Signal that will be emitted when processing is finished and Output param is NOT SET.
     * @param output                Result image.
     */
    void finished(const QImage &output);

    /**
     * @brief finished              Signal that will be emitted when processing is finished and Output param is SET.
     * @param output                Full path to the result image.
     */
    void finished(const QString &output);

private:
    QnThreadedImageLoaderPrivate* m_loader;
};

#endif // THREADED_IMAGE_LOADER_H
