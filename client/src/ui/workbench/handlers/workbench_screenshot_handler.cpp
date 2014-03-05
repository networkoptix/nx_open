#include "workbench_screenshot_handler.h"

#include <QtGui/QImageWriter>
#include <QtGui/QPainter>

#include <QtWidgets/QAction>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QMessageBox>

#include <camera/cam_display.h>
#include <camera/single_thumbnail_loader.h>

#include <client/client_settings.h>

#include <core/resource/file_processor.h>

#include <transcoding/filters/contrast_image_filter.h>
#include <transcoding/filters/fisheye_image_filter.h>

#include <ui/actions/action_manager.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/workbench/workbench_item.h>

#include <utils/common/string.h>
#include <utils/common/environment.h>
#include <utils/common/warnings.h>

//#define QN_SCREENSHOT_DEBUG
#ifdef QN_SCREENSHOT_DEBUG
#   define TRACE(...) qDebug() << __VA_ARGS__
#else
#   define TRACE(...)
#endif

namespace {
    void drawTimeStamp(QImage &image, Qn::Corner position, const QString &timestamp) {
        if (position == Qn::NoCorner)
            return;

        QScopedPointer<QPainter> painter(new QPainter(&image));
        QRect rect(image.rect());

        QFont font;
        font.setPixelSize(qMax(rect.height() / 20, 12));

        QFontMetrics fm(font);
        QSize size = fm.size(0, timestamp);
        int spacing = 2;

        painter->setPen(Qt::black);
        painter->setBrush(Qt::white);
        painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::HighQualityAntialiasing);

        QPainterPath path;
        int x = rect.x();
        int y = rect.y();

        switch (position) {
        case Qn::TopLeftCorner:
        case Qn::BottomLeftCorner:
            x += rect.x() + spacing * 2;
            break;
        default:
            x += rect.width() - size.width() - spacing*2;
            break;
        }

        switch (position) {
        case Qn::TopLeftCorner:
        case Qn::TopRightCorner:
            y += spacing + fm.ascent();
            break;
        default:
            y += rect.height() - fm.descent() - spacing;
            break;
        }

        path.addText(x, y, font, timestamp);
        painter->drawPath(path);
    }
}

QString QnScreenshotParameters::timeString() const {
    qint64 timeMSecs = time / 1000;
    if (isUtc)
        return QDateTime::fromMSecsSinceEpoch(timeMSecs).toString(lit("yyyy-MMM-dd_hh.mm.ss"));
    return QTime().addMSecs(timeMSecs).toString(lit("hh.mm.ss"));
}


// -------------------------------------------------------------------------- //
// QnScreenshotLoader
// -------------------------------------------------------------------------- //
QnScreenshotLoader::QnScreenshotLoader(const QnScreenshotParameters& parameters, QObject *parent):
    QnImageProvider(parent),
    m_parameters(parameters),
    m_isReady(false)
{
}

QnScreenshotLoader::~QnScreenshotLoader() {
    return;
}

void QnScreenshotLoader::setBaseProvider(QnImageProvider *imageProvider) {
    m_baseProvider.reset(imageProvider);
    if (!imageProvider)
        return;

    connect(imageProvider, &QnImageProvider::imageChanged, this, &QnScreenshotLoader::at_imageLoaded);
    imageProvider->loadAsync();
}

QImage QnScreenshotLoader::image() const {
    if (!m_baseProvider)
        return QImage();
    return m_baseProvider->image();
}

QnScreenshotParameters QnScreenshotLoader::parameters() const {
    return m_parameters;
}

void QnScreenshotLoader::setParameters(const QnScreenshotParameters &parameters) {
    m_parameters = parameters;
}

void QnScreenshotLoader::doLoadAsync() {
    m_isReady = true;
    if (image().isNull())
        return;

    emit imageChanged(image());
    m_isReady = false;
}

void QnScreenshotLoader::at_imageLoaded(const QImage &image) {
    if (!m_isReady)
        return;
    emit imageChanged(image);
    m_isReady = false;
}


// -------------------------------------------------------------------------- //
// QnWorkbenchScreenshotHandler
// -------------------------------------------------------------------------- //
QnWorkbenchScreenshotHandler::QnWorkbenchScreenshotHandler(QObject *parent): 
    QObject(parent), 
    QnWorkbenchContextAware(parent) 
{
    connect(action(Qn::TakeScreenshotAction), &QAction::triggered, this, &QnWorkbenchScreenshotHandler::at_takeScreenshotAction_triggered);
}

QnImageProvider* QnWorkbenchScreenshotHandler::getLocalScreenshotProvider(QnScreenshotParameters &parameters, QnResourceDisplay *display) {
    if (!display)
        return NULL;

    QList<QImage> images;

    TRACE("SCREENSHOT DEWARPING" << parameters.itemDewarpingParams.enabled << parameters.itemDewarpingParams.xAngle << parameters.itemDewarpingParams.yAngle << parameters.itemDewarpingParams.fov);

    QnConstResourceVideoLayoutPtr layout = display->videoLayout();
    bool anyQuality = layout->channelCount() > 1;   // screenshots for panoramic cameras will be done locally
    for (int i = 0; i < layout->channelCount(); ++i) {
        QImage channelImage = display->camDisplay()->getScreenshot(i, parameters.imageCorrectionParams, parameters.mediaDewarpingParams, parameters.itemDewarpingParams, anyQuality);
        if (channelImage.isNull())
            return NULL;    // async remote screenshot provider will be used
        images.push_back(channelImage);
    }
    QSize channelSize = images[0].size();
    QSize totalSize = QnGeometry::cwiseMul(channelSize, layout->size());

    QImage screenshot(totalSize.width(), totalSize.height(), QImage::Format_ARGB32);
    screenshot.fill(qRgba(0, 0, 0, 0));

    QScopedPointer<QPainter> painter(new QPainter(&screenshot));
    painter->setCompositionMode(QPainter::CompositionMode_Source);
    for (int i = 0; i < layout->channelCount(); ++i) {
        painter->drawImage(
                    QnGeometry::cwiseMul(layout->position(i), channelSize),
                    images[i]
                    );
    }

    // avoiding post-processing duplication
    parameters.imageCorrectionParams.enabled = false;
    parameters.mediaDewarpingParams.enabled = false;
    parameters.itemDewarpingParams.enabled = false;

    return new QnBasicImageProvider(screenshot);
}

void QnWorkbenchScreenshotHandler::at_takeScreenshotAction_triggered() {
    QnActionParameters actionParameters = menu()->currentParameters(sender());
    QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget *>(actionParameters.widget());
    if (!widget)
        return;

    QnResourceDisplayPtr display = widget->display();
    if (display->videoLayout()->channelCount() == 0) {
        qnWarning("No channels in resource '%1' of type '%2'.", widget->resource()->toResource()->getName(), widget->resource()->toResource()->metaObject()->className());
        return;
    }

    QnScreenshotParameters parameters;
    parameters.time = display->camDisplay()->getCurrentTime();
    parameters.isUtc = widget->resource()->toResource()->flags() & QnResource::utc;
    parameters.filename = actionParameters.argument<QString>(Qn::FileNameRole);
    parameters.timestampPosition = qnSettings->timestampCorner();
    parameters.itemDewarpingParams = widget->item()->dewarpingParams();
    parameters.mediaDewarpingParams = widget->dewarpingParams();
    parameters.imageCorrectionParams = widget->item()->imageEnhancement();
    parameters.zoomRect = parameters.itemDewarpingParams.enabled ? QRectF() : widget->zoomRect();

    QnImageProvider* imageProvider = getLocalScreenshotProvider(parameters, display.data());
    if (!imageProvider)
        imageProvider = QnSingleThumbnailLoader::newInstance(widget->resource()->toResourcePtr(), parameters.time);
    QnScreenshotLoader* loader = new QnScreenshotLoader(parameters, this);
    connect(loader, &QnImageProvider::imageChanged, this,   &QnWorkbenchScreenshotHandler::at_imageLoaded);
    loader->setBaseProvider(imageProvider); // preload screenshot here

    if(parameters.filename.isEmpty()) {
        QString previousDir = qnSettings->lastScreenshotDir();
        if (previousDir.isEmpty())
            previousDir = qnSettings->mediaFolder();

        QString suggestion = replaceNonFileNameCharacters(widget->resource()->toResource()->getName(), QLatin1Char('_'))
                + QLatin1Char('_') + parameters.timeString();
        suggestion = QnEnvironment::getUniqueFileName(previousDir, suggestion);

        QString filterSeparator = lit(";;");
        QString pngFileFilter = tr("PNG Image (*.png)");
        QString jpegFileFilter = tr("JPEG Image (*.jpg)");

        QString allowedFormatFilter = pngFileFilter;

        if (QImageWriter::supportedImageFormats().contains("jpg") ) {
            allowedFormatFilter += filterSeparator;
            allowedFormatFilter += jpegFileFilter;
        }

        QScopedPointer<QnCustomFileDialog> dialog(new QnCustomFileDialog(
            mainWindow(),
            tr("Save Screenshot As..."),
            suggestion,
            allowedFormatFilter
        ));

        dialog->setFileMode(QFileDialog::AnyFile);
        dialog->setAcceptMode(QFileDialog::AcceptSave);

        QComboBox* comboBox = new QComboBox(dialog.data());
        comboBox->addItem(tr("No timestamp"), static_cast<int>(Qn::NoCorner));
        comboBox->addItem(tr("Top left corner"), static_cast<int>(Qn::TopLeftCorner));
        comboBox->addItem(tr("Top right corner"), static_cast<int>(Qn::TopRightCorner));
        comboBox->addItem(tr("Bottom left corner"), static_cast<int>(Qn::BottomLeftCorner));
        comboBox->addItem(tr("Bottom right corner"), static_cast<int>(Qn::BottomRightCorner));
        comboBox->setCurrentIndex(comboBox->findData(parameters.timestampPosition, Qt::UserRole, Qt::MatchExactly));

        dialog->addWidget(new QLabel(tr("Timestamp:"), dialog.data()));
        dialog->addWidget(comboBox, false);

        if (!dialog->exec())
            return;

        QString fileName = dialog->selectedFile();
        if (fileName.isEmpty())
            return;

        QString selectedExtension = dialog->selectedExtension();

        if (!fileName.toLower().endsWith(selectedExtension)) {
            fileName += selectedExtension;

            if (QFile::exists(fileName)) {
                QMessageBox::StandardButton button = QMessageBox::information(
                    mainWindow(),
                    tr("Save As"),
                    tr("File '%1' already exists. Do you want to overwrite it?").arg(QFileInfo(fileName).completeBaseName()),
                    QMessageBox::Ok | QMessageBox::Cancel
                );
                if(button == QMessageBox::Cancel)
                    return;
            }
        }

        if (QFile::exists(fileName) && !QFile::remove(fileName)) {
            QMessageBox::critical(
                mainWindow(),
                tr("Could not overwrite file"),
                tr("File '%1' is used by another process. Please enter another name.").arg(QFileInfo(fileName).completeBaseName()),
                QMessageBox::Ok
            );
            return;
        }

        parameters.filename = fileName;
        parameters.timestampPosition = static_cast<Qn::Corner>(comboBox->itemData(comboBox->currentIndex()).value<int>());
        loader->setParameters(parameters); //update changed fields
    }
    loader->loadAsync();

    qnSettings->setLastScreenshotDir(QFileInfo(parameters.filename).absolutePath());
    qnSettings->setTimestampCorner(parameters.timestampPosition);
}

//TODO: #GDM may be we should incapsulate in some other object to get parameters and clear connection if user cancelled the process?
void QnWorkbenchScreenshotHandler::at_imageLoaded(const QImage &image) {

    QnScreenshotLoader* loader = dynamic_cast<QnScreenshotLoader*>(sender());
    if (!loader)
        return;
    loader->deleteLater();

    QnScreenshotParameters parameters = loader->parameters();

    QImage resized;
    if (!parameters.zoomRect.isNull()) {
        resized = QImage(image.width() * parameters.zoomRect.width(), image.height() * parameters.zoomRect.height(), QImage::Format_ARGB32);
        resized.fill(qRgba(0, 0, 0, 0));

        QScopedPointer<QPainter> painter(new QPainter(&resized));
        painter->setCompositionMode(QPainter::CompositionMode_Source);
        painter->drawImage(QPointF(0, 0), image, QnGeometry::cwiseMul(parameters.zoomRect, image.size()));
    } else {
        resized = image;
    }

    QScopedPointer<CLVideoDecoderOutput> frame(new CLVideoDecoderOutput(resized));
    if (parameters.imageCorrectionParams.enabled) {
        QnContrastImageFilter filter(parameters.imageCorrectionParams);
        filter.updateImage(frame.data(), QRectF(0.0, 0.0, 1.0, 1.0));
    }

    if (parameters.mediaDewarpingParams.enabled && parameters.itemDewarpingParams.enabled) {
        QnFisheyeImageFilter filter(parameters.mediaDewarpingParams, parameters.itemDewarpingParams);
        filter.updateImage(frame.data(), QRectF(0.0, 0.0, 1.0, 1.0));
    }

    QImage timestamped = frame->toImage();
    drawTimeStamp(timestamped, parameters.timestampPosition, parameters.timeString());

    QString filename = loader->parameters().filename;

    if (!timestamped.save(filename)) {
        QMessageBox::critical(
            mainWindow(),
            tr("Could not save screenshot"),
            tr("An error has occurred while saving screenshot '%1'.").arg(QFileInfo(filename).completeBaseName())
        );
        return;
    }

    QnFileProcessor::createResourcesForFile(filename);
}
