#include "threaded_image_loader.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

#include <ui/common/image_processing.h>

QnThreadedImageLoaderPrivate::QnThreadedImageLoaderPrivate() :
    QObject(),
    m_aspectRatio(0.0),
    m_transformationMode(Qt::SmoothTransformation),
    m_flags(Qn::DownscaleOnly | Qn::CropToTargetAspectRatio)
{
}

QnThreadedImageLoaderPrivate::~QnThreadedImageLoaderPrivate() {

}

void QnThreadedImageLoaderPrivate::setSize(const QSize &size) {
    m_size = size;
}

void QnThreadedImageLoaderPrivate::setAspectRatio(const qreal aspectRatio) {
    m_aspectRatio = aspectRatio;
}

void QnThreadedImageLoaderPrivate::setTransformationMode(const Qt::TransformationMode mode) {
    m_transformationMode = mode;
}

void QnThreadedImageLoaderPrivate::setFlags(const Qn::ImageLoaderFlags flags) {
    m_flags = flags;
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

void QnThreadedImageLoaderPrivate::start()
{
    if (!m_inputFilename.isEmpty())
    {
        if (!m_input.load(m_inputFilename))
        {
            // Workaround for Qt Mac OS bug, when QImage::load sometimes can't
            // load image from file directly.
            QFile imageFile(m_inputFilename);
            if (imageFile.open(QIODevice::ReadOnly))
                m_input = QImage::fromData(imageFile.readAll());
        }
    }

    QImage output = m_input;
    if (!output.isNull() && !qFuzzyIsNull(m_aspectRatio) && (m_flags & Qn::CropToTargetAspectRatio)) {
        output = cropToAspectRatio(output, m_aspectRatio);
    }

    if (!output.isNull() && m_size.isValid() && !m_size.isNull()) {
        QSize targetSize = m_size;
        if (m_flags & Qn::TouchSizeFromOutside) {
            qreal aspectRatio = (qreal)output.width() / (qreal)output.height();
            if (output.width() > output.height()) {
                targetSize.setWidth(qRound((qreal)targetSize.height() * aspectRatio));
            } else {
                targetSize.setHeight(qRound((qreal)targetSize.width() / aspectRatio));
            }

        }

        if (m_flags & Qn::DownscaleOnly)
            targetSize = targetSize.boundedTo(output.size());

        if (targetSize != output.size())
            output = output.scaled(targetSize, Qt::KeepAspectRatio, m_transformationMode);
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

//---------------  QnThreadedImageLoader ---------------------//

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

void QnThreadedImageLoader::setAspectRatio(const qreal aspectRatio) {
    m_loader->setAspectRatio(aspectRatio);
}

void QnThreadedImageLoader::setTransformationMode(const Qt::TransformationMode mode) {
    m_loader->setTransformationMode(mode);
}

void QnThreadedImageLoader::setFlags(const Qn::ImageLoaderFlags flags) {
    m_loader->setFlags(flags);
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


