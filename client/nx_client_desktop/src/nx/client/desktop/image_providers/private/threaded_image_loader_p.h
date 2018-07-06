#pragma once

#include <QtCore/QObject>
#include <QtCore/QSize>

#include <QtGui/QImage>

#include "../threaded_image_loader.h"

#include <utils/common/aspect_ratio.h>

namespace nx {
namespace client {
namespace desktop {

/**
* Private class. DO NOT USE it in your modules.
*/
class ThreadedImageLoaderPrivate: public QObject
{
    Q_OBJECT


public:
    explicit ThreadedImageLoaderPrivate(QObject* parent = nullptr);
    virtual ~ThreadedImageLoaderPrivate() override;

    void setSize(const QSize& size);
    void setAspectRatio(const QnAspectRatio& aspectRatio);
    void setTransformationMode(Qt::TransformationMode mode);
    void setFlags(ThreadedImageLoader::Flags flags);

    void setInput(const QImage& input);
    void setInput(const QString& filename);
    void setOutput(const QString& filename);

    void start();

signals:
    void imageLoaded(const QImage& output);
    void imageSaved(const QString& output);

private:
    QImage m_input;
    QSize m_size;
    QnAspectRatio m_aspectRatio;
    Qt::TransformationMode m_transformationMode;
    QString m_inputFilename;
    QString m_outputFilename;
    ThreadedImageLoader::Flags m_flags;
};

} // namespace desktop
} // namespace client
} // namespace nx
