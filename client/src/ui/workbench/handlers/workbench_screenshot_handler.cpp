#include "workbench_screenshot_handler.h"

#include <QtCore/QTimer>

#include <QtGui/QImageWriter>
#include <QtGui/QPainter>

#include <QtWidgets/QAction>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QMessageBox>

#include <camera/cam_display.h>
#include <camera/single_thumbnail_loader.h>

#include <client/client_settings.h>

#include <core/resource/file_processor.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource_management/resource_pool.h>

#include <transcoding/filters/contrast_image_filter.h>
#include <transcoding/filters/fisheye_image_filter.h>

#include <ui/actions/action_manager.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/dialogs/progress_dialog.h>
#include <ui/dialogs/workbench_state_dependent_dialog.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>
#include <ui/workbench/workbench_context.h>

#include <utils/common/string.h>
#include <utils/common/environment.h>
#include <utils/common/warnings.h>
#include "transcoding/filters/fisheye_image_filter.h"
#include "transcoding/filters/filter_helper.h"
#include "transcoding/filters/abstract_image_filter.h"

//#define QN_SCREENSHOT_DEBUG
#ifdef QN_SCREENSHOT_DEBUG
#   define TRACE(...) qDebug() << __VA_ARGS__
#else
#   define TRACE(...)
#endif

namespace {
    const int showProgressDelay = 1500; // 1.5 sec
    const int minProgressDisplayTime = 1000; // 1 sec
}

QString QnScreenshotParameters::timeString() const {
    qint64 timeMSecs = time / 1000;
    if (isUtc)
        return datetimeSaveDialogSuggestion(QDateTime::fromMSecsSinceEpoch(timeMSecs));
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

    QImage img = image();
    if (img.isNull())
        return; //waiting for image to be loaded

    // image is already loaded within base provider
    emit imageChanged(img);
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
    QnWorkbenchContextAware(parent),
    m_screenshotProgressDialog(0),
    m_progressShowTime(0),
    m_canceled(false)
{
    connect(action(Qn::TakeScreenshotAction), &QAction::triggered, this, &QnWorkbenchScreenshotHandler::at_takeScreenshotAction_triggered);
}

QnImageProvider* QnWorkbenchScreenshotHandler::getLocalScreenshotProvider(QnScreenshotParameters &parameters, QnResourceDisplay *display) {
    if (!display)
        return NULL;

    TRACE("SCREENSHOT DEWARPING" << parameters.itemDewarpingParams.enabled << parameters.itemDewarpingParams.xAngle << parameters.itemDewarpingParams.yAngle << parameters.itemDewarpingParams.fov);

    QnConstResourceVideoLayoutPtr layout = display->videoLayout();
    bool anyQuality = layout->channelCount() > 1;   // screenshots for panoramic cameras will be done locally

    const QnMediaServerResourcePtr server = qnResPool->getResourceById(display->mediaResource()->toResource()->getParentId()).dynamicCast<QnMediaServerResource>();
    if (!server || (server->getServerFlags() & Qn::SF_Edge))
        anyQuality = true; // local file or edge cameras will be done locally

    // Either tiling (pano cameras) and crop rect are handled here, so it isn't passed to image processing params

    QnImageFilterHelper imageProcessingParams;
    QnMediaResourcePtr mediaRes = display->resource().dynamicCast<QnMediaResource>();
    if (mediaRes)
        imageProcessingParams.setVideoLayout(layout);
    imageProcessingParams.setSrcRect(parameters.zoomRect);
    imageProcessingParams.setContrastParams(parameters.imageCorrectionParams);
    imageProcessingParams.setDewarpingParams(parameters.mediaDewarpingParams, parameters.itemDewarpingParams);
    imageProcessingParams.setRotation(parameters.rotationAngle);
    imageProcessingParams.setCustomAR(parameters.customAspectRatio);

    QImage screenshot = display->camDisplay()->getScreenshot(imageProcessingParams, anyQuality);
    if (screenshot.isNull())
        return NULL;

    // avoiding post-processing duplication
    parameters.imageCorrectionParams.enabled = false;
    parameters.mediaDewarpingParams.enabled = false;
    parameters.itemDewarpingParams.enabled = false;
    parameters.zoomRect = QRectF();
    parameters.customAspectRatio = 0.0;
    parameters.rotationAngle = 0;

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
    parameters.isUtc = widget->resource()->toResource()->flags() & Qn::utc;
    parameters.filename = actionParameters.argument<QString>(Qn::FileNameRole);
    parameters.timestampPosition = qnSettings->timestampCorner();
    parameters.itemDewarpingParams = widget->item()->dewarpingParams();
    parameters.mediaDewarpingParams = widget->dewarpingParams();
    parameters.imageCorrectionParams = widget->item()->imageEnhancement();
    parameters.zoomRect = parameters.itemDewarpingParams.enabled ? QRectF() : widget->zoomRect();
    parameters.customAspectRatio = display->camDisplay()->overridenAspectRatio();
    parameters.rotationAngle = widget->rotation();
    // Revert UI has 'hack'. VerticalDown fisheye option is emulated by additional rotation. But filter has built-in support for that option.
    if (parameters.itemDewarpingParams.enabled && parameters.mediaDewarpingParams.viewMode == QnMediaDewarpingParams::VerticalDown)
        parameters.rotationAngle -= 180;

		// ----------------------------------------------------- 
		// This localOffset is used to fix the issue : Bug #2988 
		// ----------------------------------------------------- 
		qint64 localOffset = 0;
		if(qnSettings->timeMode() == Qn::ServerTimeMode && parameters.isUtc)
			localOffset = context()->instance<QnWorkbenchServerTimeWatcher>()->localOffset(widget->resource(), 0);
		parameters.time += localOffset*1000;

    QnImageProvider* imageProvider = getLocalScreenshotProvider(parameters, display.data());
    if (!imageProvider)
        imageProvider = QnSingleThumbnailLoader::newInstance(widget->resource()->toResourcePtr(), parameters.time, 0);
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

        bool wasLoggedIn = !context()->user().isNull();

        QScopedPointer<QnCustomFileDialog> dialog(
            new QnWorkbenchStateDependentDialog<QnCustomFileDialog> (
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

        dialog->addWidget(tr("Timestamp:"), comboBox);

        if (!dialog->exec())
            return;

        /* Check if we were disconnected (server shut down) while the dialog was open. 
         * Skip this check if we were not logged in before. */
        if (wasLoggedIn && !context()->user())
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
                    tr("File '%1' already exists. Do you want to overwrite it?").arg(QFileInfo(fileName).fileName()),
                    QMessageBox::Ok | QMessageBox::Cancel
                );
                if(button == QMessageBox::Cancel)
                    return;
            }
        }

        /* Check if we were disconnected (server shut down) while the dialog was open. 
         * Skip this check if we were not logged in before. */
        if (wasLoggedIn && !context()->user())
            return;

        if (QFile::exists(fileName) && !QFile::remove(fileName)) {
            QMessageBox::critical(
                mainWindow(),
                tr("Could not overwrite file"),
                tr("File '%1' is used by another process. Please enter another name.").arg(QFileInfo(fileName).fileName()),
                QMessageBox::Ok
            );
            return;
        }

        /* Check if we were disconnected (server shut down) while the dialog was open. 
         * Skip this check if we were not logged in before. */
        if (wasLoggedIn && !context()->user())
            return; //TODO: #GDM triple check...

        parameters.filename = fileName;
        parameters.timestampPosition = static_cast<Qn::Corner>(comboBox->itemData(comboBox->currentIndex()).value<int>());
        loader->setParameters(parameters); //update changed fields

        showProgressDelayed(tr("Saving %1").arg(QFileInfo(fileName).fileName()));
        connect(m_screenshotProgressDialog, &QnProgressDialog::canceled, loader, &QnScreenshotLoader::deleteLater);
    }
    m_canceled = false;
    loader->loadAsync();

    qnSettings->setLastScreenshotDir(QFileInfo(parameters.filename).absolutePath());
    qnSettings->setTimestampCorner(parameters.timestampPosition);
}

//TODO: #GDM #Business may be we should encapsulate in some other object to get parameters and clear connection if user cancelled the process?
void QnWorkbenchScreenshotHandler::at_imageLoaded(const QImage &image) {
    QnScreenshotLoader* loader = dynamic_cast<QnScreenshotLoader*>(sender());
    if (!loader)
        return;
    loader->deleteLater();

    if (m_canceled)
        return;

    hideProgressDelayed();

    QnScreenshotParameters parameters = loader->parameters();
    QnImageFilterHelper transcodeParams;
    // Doing heavy filters only. This filters doesn't supported on server side for screenshots
    transcodeParams.setDewarpingParams(parameters.mediaDewarpingParams, parameters.itemDewarpingParams);
    transcodeParams.setContrastParams(parameters.imageCorrectionParams);
    transcodeParams.setTimeCorner(parameters.timestampPosition, 0);
    transcodeParams.setRotation(parameters.rotationAngle);
    transcodeParams.setSrcRect(parameters.zoomRect);
    QList<QnAbstractImageFilterPtr> filters = transcodeParams.createFilterChain(image.size());
    QImage result = image;
    if (!filters.isEmpty()) {
        QSharedPointer<CLVideoDecoderOutput> frame(new CLVideoDecoderOutput(result));
        frame->pts = parameters.time;
        for(auto filter: filters)
            frame = filter->updateImage(frame);
        result = frame->toImage();
    }

    QString filename = loader->parameters().filename;

    if (!result.save(filename)) {
        hideProgress();

        QMessageBox::critical(
            mainWindow(),
            tr("Could not save screenshot"),
            tr("An error has occurred while saving screenshot '%1'.").arg(QFileInfo(filename).fileName())
        );
        return;
    }

    QnFileProcessor::createResourcesForFile(filename);
}

void QnWorkbenchScreenshotHandler::showProgressDelayed(const QString &message) {
    if (!m_screenshotProgressDialog) {
        m_screenshotProgressDialog = new QnWorkbenchStateDependentDialog<QnProgressDialog>(mainWindow());
        m_screenshotProgressDialog->setWindowTitle(tr("Saving Screenshot..."));
        m_screenshotProgressDialog->setInfiniteProgress();
        // TODO: #dklychkov ensure concurrent screenshot saving is ok and disable modality
        m_screenshotProgressDialog->setModal(true);
        connect(m_screenshotProgressDialog, &QnProgressDialog::canceled, this, &QnWorkbenchScreenshotHandler::cancelLoading);
    }

    m_screenshotProgressDialog->setLabelText(message);

    m_progressShowTime = QDateTime::currentMSecsSinceEpoch() + showProgressDelay;
    QTimer::singleShot(showProgressDelay, this, SLOT(showProgress()));
}

void QnWorkbenchScreenshotHandler::showProgress() {
    if (m_progressShowTime == 0)
        return;

    m_progressShowTime = QDateTime::currentMSecsSinceEpoch();
    m_screenshotProgressDialog->show();
}

void QnWorkbenchScreenshotHandler::hideProgressDelayed() {
    if (!m_screenshotProgressDialog)
        return;

    if (!m_screenshotProgressDialog->isVisible()) {
        hideProgress();
        return;
    }

    int restTimeMs = minProgressDisplayTime - (QDateTime::currentMSecsSinceEpoch() - m_progressShowTime);
    if (restTimeMs < 0)
        hideProgress();
    else
        QTimer::singleShot(restTimeMs, this, SLOT(hideProgress()));
}

void QnWorkbenchScreenshotHandler::hideProgress() {
    if (!m_screenshotProgressDialog)
        return;

    m_progressShowTime = 0;
    m_screenshotProgressDialog->hide();
}

void QnWorkbenchScreenshotHandler::cancelLoading() {
    m_canceled = true;
}
