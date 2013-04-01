#ifndef THREADED_IMAGE_LOADER_H
#define THREADED_IMAGE_LOADER_H

#include <QtCore/QObject>
#include <QtCore/QSize>
#include <QtCore/QThread>

#include <QtGui/QImage>

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

    void start();
signals:
    void finished(const QImage &output);

private:
    QSize m_size;
    Qt::AspectRatioMode m_aspectMode;
    Qt::TransformationMode m_transformationMode;
    bool m_downScaleOnly;
    QImage m_input;
    QString m_inputFilename;
};


class QnThreadedImageLoader : public QObject
{
    Q_OBJECT
public:
    explicit QnThreadedImageLoader(QObject *parent = 0);
    ~QnThreadedImageLoader();

public slots:
    void setSize(const QSize &size);
    void setAspectRatioMode(const Qt::AspectRatioMode mode);
    void setTransformationMode(const Qt::TransformationMode mode);
    void setDownScaleOnly(const bool value);

    void setInput(const QImage &input);
    void setInput(const QString &filename);

    void start();
signals:
    void finished(const QImage &output);

private:
    QnThreadedImageLoaderPrivate* m_loader;
};

#endif // THREADED_IMAGE_LOADER_H
