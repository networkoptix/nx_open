#include "threaded_image_loader.h"

QnThreadedImageLoader::QnThreadedImageLoader(QObject *parent) :
    QObject(parent),
    m_aspectMode(Qt::KeepAspectRatio),
    m_transformationMode(Qt::SmoothTransformation)
{
}

QnThreadedImageLoader::~QnThreadedImageLoader() {

}

void QnThreadedImageLoader::setSize(const QSize &size) {
    m_size = size;
}

void QnThreadedImageLoader::setAspectRatioMode(const Qt::AspectRatioMode mode) {
    m_aspectMode = mode;
}

void QnThreadedImageLoader::setTransformationMode(const Qt::TransformationMode mode) {
    m_transformationMode = mode;
}

void QnThreadedImageLoader::setInput(const QImage &input) {
    m_input = input;
}

void QnThreadedImageLoader::setInput(const QString &filename) {
    m_inputFilename = filename;
}

void QnThreadedImageLoader::start() {
    if (!m_inputFilename.isEmpty()) {
        m_input.load(m_inputFilename );
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


