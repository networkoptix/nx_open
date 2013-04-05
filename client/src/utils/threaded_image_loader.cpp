#include "threaded_image_loader.h"

#include <QtCore/QFileInfo>


QnThreadedImageLoaderPrivate::QnThreadedImageLoaderPrivate() :
    QObject(),
    m_aspectMode(Qt::KeepAspectRatio),
    m_transformationMode(Qt::SmoothTransformation),
    m_downScaleOnly(true)
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

void QnThreadedImageLoaderPrivate::setDownScaleOnly(const bool value) {
    m_downScaleOnly = value;
}

void QnThreadedImageLoaderPrivate::setInput(const QImage &input) {
    m_input = input;
}

void QnThreadedImageLoaderPrivate::setInput(const QString &filename) {
    m_inputFilename = filename;
}

void QnThreadedImageLoaderPrivate::setOutput(const QString &filename) {
    m_outputFilename = filename;
}

void QnThreadedImageLoaderPrivate::start() {
    if (!m_inputFilename.isEmpty()) {
        m_input.load(m_inputFilename);
    }

    QImage output = m_input;
    if (!output.isNull() && m_size.isValid() && !m_size.isNull()) {
        if (m_downScaleOnly)
            m_size = m_size.boundedTo(output.size());

        if (m_size != output.size())
            output = output.scaled(m_size, m_aspectMode, m_transformationMode);
    }

    if (!m_outputFilename.isEmpty()) {
        QString folder = QFileInfo(m_outputFilename).absolutePath();
        QDir().mkpath(folder);
        output.save(m_outputFilename);
        emit finished(m_outputFilename);
    } else {
        emit finished(output);
    }
    delete this;
}


QnThreadedImageLoader::QnThreadedImageLoader(QObject *parent) :
    QObject(parent),
    m_loader(new QnThreadedImageLoaderPrivate())
{
    connect(m_loader, SIGNAL(finished(QImage)), this, SIGNAL(finished(QImage)));
    connect(m_loader, SIGNAL(finished(QString)), this, SIGNAL(finished(QString)));
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

void QnThreadedImageLoader::setDownScaleOnly(const bool value) {
    m_loader->setDownScaleOnly(value);
}

void QnThreadedImageLoader::setInput(const QImage &input) {
    m_loader->setInput(input);
}

void QnThreadedImageLoader::setInput(const QString &filename) {
    m_loader->setInput(filename);
}

void QnThreadedImageLoader::setOutput(const QString &filename) {
    m_loader->setOutput(filename);
}

void QnThreadedImageLoader::start() {
    QThread *thread = new QThread();
    m_loader->moveToThread( thread );

    connect(thread, SIGNAL(started()), m_loader, SLOT(start()));
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));

    connect(m_loader, SIGNAL(finished(QImage)), thread, SLOT(quit()));

    thread->start();
}


