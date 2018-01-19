#include "threaded_image_loader.h"
#include "private/threaded_image_loader_p.h"

#include <QtCore/QDir>
#include <QtWidgets/QApplication>

namespace nx {
namespace client {
namespace desktop {

ThreadedImageLoader::ThreadedImageLoader(QObject *parent):
    QObject(parent),
    m_loader(new ThreadedImageLoaderPrivate())
{
    connect(m_loader, &ThreadedImageLoaderPrivate::imageLoaded, this,
        &ThreadedImageLoader::imageLoaded);
    connect(m_loader, &ThreadedImageLoaderPrivate::imageSaved, this,
        &ThreadedImageLoader::imageSaved);
}

ThreadedImageLoader::~ThreadedImageLoader()
{
}

void ThreadedImageLoader::setSize(const QSize& size)
{
    m_loader->setSize(size);
}

void ThreadedImageLoader::setAspectRatio(const QnAspectRatio& aspectRatio)
{
    m_loader->setAspectRatio(aspectRatio);
}

void ThreadedImageLoader::setTransformationMode(const Qt::TransformationMode mode)
{
    m_loader->setTransformationMode(mode);
}

void ThreadedImageLoader::setFlags(Flags flags)
{
    m_loader->setFlags(flags);
}

void ThreadedImageLoader::setInput(const QImage& input)
{
    m_loader->setInput(input);
}

void ThreadedImageLoader::setInput(const QString& filename)
{
    m_loader->setInput(filename);
}

void ThreadedImageLoader::setOutput(const QString& filename)
{
    m_loader->setOutput(filename);
}

void ThreadedImageLoader::start()
{
    auto thread = new QThread();
    m_loader->moveToThread(thread);

    connect(thread, &QThread::started, m_loader, &ThreadedImageLoaderPrivate::start);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(thread, &QThread::finished, m_loader, &QObject::deleteLater);

    auto cleanup = [this, thread] { thread->quit(); };

    connect(m_loader, &ThreadedImageLoaderPrivate::imageLoaded, this, cleanup);
    connect(m_loader, &ThreadedImageLoaderPrivate::imageSaved, this, cleanup);

    thread->start();
}

} // namespace desktop
} // namespace client
} // namespace nx
