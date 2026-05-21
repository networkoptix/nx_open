// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "threaded_image_loader.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QSaveFile>
#include <QtCore/QThread>
#include <QtGui/QImageReader>

#include <nx/utils/log/log.h>
#include <ui/common/image_processing.h>
#include <utils/common/aspect_ratio.h>

namespace nx::vms::client::desktop {

namespace {

// The default QImageReader allocation limit is 128 MB, which is not enough in practice. Introduce
// a new allocation limit that is three times bigger than the default one.
const auto kImageAllocationLimit = 384;

} // namespace

struct ThreadedImageLoader::Parameters
{
    QImage input;
    QSize size;
    QnAspectRatio aspectRatio;
    Qt::TransformationMode transformationMode = Qt::SmoothTransformation;
    QString inputFilename;
    QString outputFilename;
    ThreadedImageLoader::Flags flags =
        ThreadedImageLoader::DownscaleOnly | ThreadedImageLoader::CropToTargetAspectRatio;
};

struct ThreadedImageLoader::ThreadedImageLoaderWorker: public QObject
{
    Q_OBJECT

public:
    ThreadedImageLoader::Parameters parameters;

    void run();

signals:
    void imageSaved(const QString& savedPath);
    void imageLoaded(const QImage& image);
    void finished();
};

void ThreadedImageLoader::ThreadedImageLoaderWorker::run()
{
    if (!parameters.inputFilename.isEmpty())
    {
        // TODO: #ynikitenkov Store image file in layout with correct extension.
        QImageReader reader(parameters.inputFilename);
        reader.setDecideFormatFromContent(true);
        reader.read(&parameters.input);
    }

    QImage output = parameters.input;
    if (!output.isNull()
        && parameters.aspectRatio.isValid()
        && parameters.flags.testFlag(ThreadedImageLoader::CropToTargetAspectRatio))
    {
        output = cropToAspectRatio(output, parameters.aspectRatio);
    }

    if (!output.isNull()
        && parameters.size.isValid()
        && !parameters.size.isNull())
    {
        QSize targetSize = parameters.size;
        if (parameters.flags.testFlag(ThreadedImageLoader::TouchSizeFromOutside))
        {
            qreal aspectRatio = (qreal)output.width() / (qreal)output.height();
            if (output.width() > output.height())
                targetSize.setWidth(qRound((qreal)targetSize.height() * aspectRatio));
            else
                targetSize.setHeight(qRound((qreal)targetSize.width() / aspectRatio));
        }

        if (parameters.flags.testFlag(ThreadedImageLoader::DownscaleOnly))
            targetSize = targetSize.boundedTo(output.size());

        if (targetSize != output.size())
        {
            output = output.scaled(
                targetSize, Qt::KeepAspectRatio, parameters.transformationMode);
        }
    }

    if (!parameters.outputFilename.isEmpty())
    {
        QString savedPath;
        if (!output.isNull())
        {
            const auto folder = QFileInfo(parameters.outputFilename).absolutePath();
            if (QDir().mkpath(folder))
            {
                // Atomic write: QSaveFile writes to a temporary file and renames on commit(),
                // so the destination is either left untouched or replaced with a complete image.
                QSaveFile file(parameters.outputFilename);
                const auto format = QFileInfo(parameters.outputFilename).suffix().toUtf8();
                if (!file.open(QIODevice::WriteOnly))
                {
                    NX_WARNING(this, "Failed to open file for writing: %1 (%2)",
                        parameters.outputFilename, file.errorString());
                }
                else if (!output.save(&file, format.isEmpty() ? nullptr : format.constData()))
                {
                    NX_WARNING(this, "Failed to encode image to file: %1",
                        parameters.outputFilename);
                    // QSaveFile destructor discards the temporary file when commit() is not called.
                }
                else if (!file.commit())
                {
                    NX_WARNING(this, "Failed to commit file: %1 (%2)",
                        parameters.outputFilename, file.errorString());
                }
                else
                {
                    savedPath = parameters.outputFilename;
                }
            }
            else
            {
                NX_WARNING(this, "Failed to create directory: %1", folder);
            }
        }
        emit imageSaved(savedPath);
    }
    else
    {
        emit imageLoaded(output);
    }

    emit finished();
}

ThreadedImageLoader::ThreadedImageLoader(QObject* parent):
    QObject(parent),
    parameters(new Parameters())
{
    QImageReader::setAllocationLimit(kImageAllocationLimit);
}

ThreadedImageLoader::~ThreadedImageLoader()
{
}

void ThreadedImageLoader::setSize(const QSize& size)
{
    parameters->size = size;
}

void ThreadedImageLoader::setAspectRatio(const QnAspectRatio& aspectRatio)
{
    parameters->aspectRatio = aspectRatio;
}

void ThreadedImageLoader::setTransformationMode(const Qt::TransformationMode mode)
{
    parameters->transformationMode = mode;
}

void ThreadedImageLoader::setFlags(Flags flags)
{
    parameters->flags = flags;
}

void ThreadedImageLoader::setInput(const QImage& input)
{
    parameters->input = input;
}

void ThreadedImageLoader::setInput(const QString& filename)
{
    parameters->inputFilename = filename;
}

void ThreadedImageLoader::setOutput(const QString& filename)
{
    parameters->outputFilename = filename;
}

void ThreadedImageLoader::start()
{
    auto* thread = new QThread;
    auto* worker = new ThreadedImageLoaderWorker;
    worker->parameters = *parameters;
    worker->moveToThread(thread);

    // Worker->loader connections are auto-detected as QueuedConnection (different threads). If
    // the loader is destroyed before the worker emits, ~QObject disconnects them and the emit
    // becomes a no-op; the worker itself never touches the loader.
    connect(thread, &QThread::started, worker, &ThreadedImageLoaderWorker::run);
    connect(worker, &ThreadedImageLoaderWorker::imageSaved,
        this, &ThreadedImageLoader::imageSaved);
    connect(worker, &ThreadedImageLoaderWorker::imageLoaded,
        this, &ThreadedImageLoader::imageLoaded);
    connect(worker, &ThreadedImageLoaderWorker::finished, thread, &QThread::quit);
    connect(worker, &ThreadedImageLoaderWorker::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}

} // namespace nx::vms::client::desktop

#include "threaded_image_loader.moc"
