#include "threaded_image_loader.h"


QnThreadedImageLoaderPrivate::QnThreadedImageLoaderPrivate() :
    QObject(),
    m_aspectMode(Qt::KeepAspectRatio),
    m_transformationMode(Qt::SmoothTransformation)
{
}

QnThreadedImageLoaderPrivate::~QnThreadedImageLoaderPrivate() {

}

void QnThreadedImageLoaderPrivate::setSize(const QSize &size) {
    m_size = size;
}

void QnThreadedImageLoaderPrivate::setAspectRatioMode(const Qt::AspectRatioMode mode) {
    m_aspectMode = mode;
}

void QnThreadedImageLoaderPrivate::setTransformationMode(const Qt::TransformationMode mode) {
    m_transformationMode = mode;
}

void QnThreadedImageLoaderPrivate::setInput(const QImage &input) {
    m_input = input;
}

void QnThreadedImageLoaderPrivate::setInput(const QString &filename) {
    m_inputFilename = filename;
}

void QnThreadedImageLoaderPrivate::start() {
    if (!m_inputFilename.isEmpty()) {
        m_input.load(m_inputFilename);
    }

    if (m_input.isNull()) {
        emit error();
        delete this;
        return;
    }

    QImage output = m_input.scaled(m_size, m_aspectMode, m_transformationMode);
    emit finished(output);
    delete this;
}


QnThreadedImageLoader::QnThreadedImageLoader(QObject *parent) :
    QObject(parent),
    m_loader(new QnThreadedImageLoaderPrivate())
{
    connect(m_loader, SIGNAL(finished(QImage)), this, SIGNAL(finished(QImage)));
    connect(m_loader, SIGNAL(error()), this, SIGNAL(error()));
}

QnThreadedImageLoader::~QnThreadedImageLoader() {

}

void QnThreadedImageLoader::setSize(const QSize &size) {
    m_loader->setSize(size);
}

void QnThreadedImageLoader::setAspectRatioMode(const Qt::AspectRatioMode mode) {
    m_loader->setAspectRatioMode(mode);
}

void QnThreadedImageLoader::setTransformationMode(const Qt::TransformationMode mode) {
    m_loader->setTransformationMode(mode);
}

void QnThreadedImageLoader::setInput(const QImage &input) {
    m_loader->setInput(input);
}

void QnThreadedImageLoader::setInput(const QString &filename) {
    m_loader->setInput(filename);
}

void QnThreadedImageLoader::start() {
    QThread *thread = new QThread();
    m_loader->moveToThread( thread );

    connect(thread, SIGNAL(started()), m_loader, SLOT(start()));

    connect(m_loader, SIGNAL(finished(QImage)), thread, SLOT(quit()));
    connect(m_loader, SIGNAL(error()), thread, SLOT(quit()));

    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));

    thread->start();
}


