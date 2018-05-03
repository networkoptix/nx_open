#include "threaded_image_loader_p.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <QtGui/QImageReader>

#include <ui/common/image_processing.h>

namespace nx {
namespace client {
namespace desktop {

ThreadedImageLoaderPrivate::ThreadedImageLoaderPrivate(QObject* parent):
    QObject(parent),
    m_transformationMode(Qt::SmoothTransformation),
    m_flags(ThreadedImageLoader::DownscaleOnly | ThreadedImageLoader::CropToTargetAspectRatio)
{
}

ThreadedImageLoaderPrivate::~ThreadedImageLoaderPrivate()
{
}

void ThreadedImageLoaderPrivate::setSize(const QSize &size)
{
    m_size = size;
}

void ThreadedImageLoaderPrivate::setAspectRatio(const QnAspectRatio& aspectRatio)
{
    m_aspectRatio = aspectRatio;
}

void ThreadedImageLoaderPrivate::setTransformationMode(const Qt::TransformationMode mode)
{
    m_transformationMode = mode;
}

void ThreadedImageLoaderPrivate::setFlags(const ThreadedImageLoader::Flags flags)
{
    m_flags = flags;
}

void ThreadedImageLoaderPrivate::setInput(const QImage &input)
{
    m_input = input;
}

void ThreadedImageLoaderPrivate::setInput(const QString &filename)
{
    m_inputFilename = filename;
}

void ThreadedImageLoaderPrivate::setOutput(const QString &filename)
{
    m_outputFilename = filename;
}

void ThreadedImageLoaderPrivate::start()
{
    if (!m_inputFilename.isEmpty())
    {
        // TODO: #ynikitenkov Store image file in layout with correct extension
        QImageReader reader(m_inputFilename);
        reader.setDecideFormatFromContent(true);
        reader.read(&m_input);
    }

    QImage output = m_input;
    if (!output.isNull()
        && m_aspectRatio.isValid()
        && m_flags.testFlag(ThreadedImageLoader::CropToTargetAspectRatio))
    {
        output = cropToAspectRatio(output, m_aspectRatio);
    }

    if (!output.isNull() && m_size.isValid() && !m_size.isNull())
    {
        QSize targetSize = m_size;
        if (m_flags.testFlag(ThreadedImageLoader::TouchSizeFromOutside))
        {
            qreal aspectRatio = (qreal)output.width() / (qreal)output.height();
            if (output.width() > output.height())
            {
                targetSize.setWidth(qRound((qreal)targetSize.height() * aspectRatio));
            }
            else
            {
                targetSize.setHeight(qRound((qreal)targetSize.width() / aspectRatio));
            }

        }

        if (m_flags.testFlag(ThreadedImageLoader::DownscaleOnly))
            targetSize = targetSize.boundedTo(output.size());

        if (targetSize != output.size())
            output = output.scaled(targetSize, Qt::KeepAspectRatio, m_transformationMode);
    }

    if (!m_outputFilename.isEmpty())
    {
        const auto folder = QFileInfo(m_outputFilename).absolutePath();
        QDir().mkpath(folder);
        output.save(m_outputFilename);
        emit imageSaved(m_outputFilename);
    }
    else
    {
        emit imageLoaded(output);
    }
}

} // namespace desktop
} // namespace client
} // namespace nx
