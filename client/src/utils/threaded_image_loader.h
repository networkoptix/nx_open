#ifndef THREADED_IMAGE_LOADER_H
#define THREADED_IMAGE_LOADER_H

#include <QtCore/QObject>
#include <QtCore/QSize>
#include <QtCore/QThread>

#include <QtGui/QImage>

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
    void setAspectRatioMode(const Qt::AspectRatioMode mode);
    void setTransformationMode(const Qt::TransformationMode mode);
    void setDownScaleOnly(const bool value);

    void setInput(const QImage &input);
    void setInput(const QString &filename);
    void setOutput(const QString &filename);

    void start();
signals:
    void finished(const QImage &output);
    void finished(const QString &output);

private:
    QSize m_size;
    Qt::AspectRatioMode m_aspectMode;
    Qt::TransformationMode m_transformationMode;
    bool m_downScaleOnly;
    QImage m_input;
    QString m_inputFilename;
    QString m_outputFilename;
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
     * @brief setAspectRatioMode    Set aspect ratio used in resize. Default value is Qt::KeepAspectRatio.
     * @param mode                  Qt::AspectRatioMode
     */
    void setAspectRatioMode(const Qt::AspectRatioMode mode);

    /**
     * @brief setTransformationMode Set transformation mode used in resize. Default value is Qt::SmoothTransformation.
     * @param mode                  Qt::TransformationMode
     */
    void setTransformationMode(const Qt::TransformationMode mode);

    /**
     * @brief setDownScaleOnly      Set downscale-only flag. If set image will not be upscaled. Default value is True.
     * @param value                 Bool value
     */
    void setDownScaleOnly(const bool value);

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
