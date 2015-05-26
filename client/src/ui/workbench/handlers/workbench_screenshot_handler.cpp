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
#include <client/client_runtime_settings.h>

#include <core/resource/file_processor.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/camera_resource.h>
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

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>

#include <utils/common/string.h>
#include <utils/common/environment.h>
#include <utils/common/warnings.h>
#include <utils/aspect_ratio.h>

#include "transcoding/filters/fisheye_image_filter.h"
#include "transcoding/filters/filter_helper.h"
#include "transcoding/filters/abstract_image_filter.h"

namespace {
    const int showProgressDelay = 1500; // 1.5 sec
    const int minProgressDisplayTime = 1000; // 1 sec

    /* Parameter value to load latest available screenshot. */
    const qint64 latestScreenshotTime = -1;
}

QnScreenshotParameters::QnScreenshotParameters():
    time(0),
    isUtc(false),
    customAspectRatio(0.0),
    rotationAngle(0.0)
{}

QString QnScreenshotParameters::timeString() const {
    if (time == latestScreenshotTime)
        return QDateTime::currentDateTime().toString(lit("hh.mm.ss"));

    qint64 timeMSecs = adjustedTime / 1000;
    if (isUtc)
        return datetimeSaveDialogSuggestion(QDateTime::fromMSecsSinceEpoch(timeMSecs));
    return QTime(0, 0, 0, 0).addMSecs(timeMSecs).toString(lit("hh.mm.ss"));
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

QnImageProvider* QnWorkbenchScreenshotHandler::getLocalScreenshotProvider(QnMediaResourceWidget *widget, const QnScreenshotParameters &parameters) const {
    QnResourceDisplayPtr display = widget->display();

    QnConstResourceVideoLayoutPtr layout = display->videoLayout();
    bool anyQuality = layout->channelCount() > 1;   // screenshots for panoramic cameras will be done locally

    const QnMediaServerResourcePtr server = qnResPool->getResourceById<QnMediaServerResource>(display->mediaResource()->toResource()->getParentId());
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

    return new QnBasicImageProvider(screenshot);
}

qint64 QnWorkbenchScreenshotHandler::screenshotTime(QnMediaResourceWidget *widget, bool adjust) {
    QnResourceDisplayPtr display = widget->display();

    qint64 time = display->camDisplay()->getCurrentTime();
    if (time == AV_NOPTS_VALUE)
        return latestScreenshotTime;

    if (!adjust)
        return time;

    bool isUtc = widget->resource()->toResource()->flags() & Qn::utc;
    qint64 localOffset = 0;
    if(qnSettings->timeMode() == Qn::ServerTimeMode && isUtc)
        localOffset = context()->instance<QnWorkbenchServerTimeWatcher>()->localOffset(widget->resource(), 0);

    time += localOffset*1000;
    return time;
}

void QnWorkbenchScreenshotHandler::takeDebugScreenshotsSet(QnMediaResourceWidget *widget) {

    QStack<QString> keyStack;
    keyStack.push(qnSettings->lastScreenshotDir() + lit("/"));
    keyStack.push(widget->resource()->toResource()->getName());

    struct Key {
        Key(QStack<QString> &stack, const QString &value):
            m_stack(stack)
        {
            m_stack.push(value);
        }

        ~Key() {
            m_stack.pop();
        }

        QStack<QString> &m_stack;
    };

    auto path = [&keyStack]() {
        QString result;
        for (const QString &e: keyStack.toList())
            result += e;
        return result;
    };
    
    int count = 1;
    int current = 0;

    QList<QString> formats;
    formats << lit(".png");
    if (QImageWriter::supportedImageFormats().contains("jpg"))
        formats << lit(".jpg");
    count *= formats.size();

    typedef QPair<QString, float> ar_type;
    QList<ar_type> ars;
    ars << ar_type(QString(), 0.0);
    for (const QnAspectRatio &ar: QnAspectRatio::standardRatios())
        ars << ar_type(ar.toString(lit("_%1x%2")), ar.toFloat());
    count*= ars.size();

    QList<int> rotations;
    for(int rotation = 0; rotation < 360; rotation += 90)
        rotations << rotation;
    count *= rotations.size();

    QList<ImageCorrectionParams> imageCorrList;
    {
        auto params = widget->item()->imageEnhancement();
        imageCorrList << params;
        params.enabled = !params.enabled;
        imageCorrList << params;
    }
    count *= imageCorrList.size();

    typedef QPair<QString, Qn::Corner> crn_type;
    QList<crn_type> tsCorners;
    tsCorners 
        << crn_type(lit("_nots"), Qn::NoCorner) 
        << crn_type(lit("_topLeft"), Qn::TopLeftCorner)
        << crn_type(lit("_topRight"), Qn::TopRightCorner)
        << crn_type(lit("_btmLeft"), Qn::BottomLeftCorner)
        << crn_type(lit("_btmRight"), Qn::BottomRightCorner);
    count *= tsCorners.size();   

    QnResourceDisplayPtr display = widget->display();

    QScopedPointer<QnProgressDialog> dialog(new QnProgressDialog(mainWindow()));
    dialog->setMaximum(count);
    dialog->setValue(current);
    dialog->setMinimumWidth(600);
    dialog->show();

    qint64 startTime = QDateTime::currentMSecsSinceEpoch();

    QnScreenshotParameters parameters;
    {       
        parameters.time = screenshotTime(widget);
        parameters.isUtc = widget->resource()->toResource()->flags() & Qn::utc;
        parameters.adjustedTime = screenshotTime(widget, true);
        Key timeKey(keyStack, lit("_") + parameters.timeString());
       
        parameters.itemDewarpingParams = widget->item()->dewarpingParams();
        parameters.mediaDewarpingParams = widget->dewarpingParams();       
        parameters.zoomRect = parameters.itemDewarpingParams.enabled ? QRectF() : widget->zoomRect();
        
        for (const ImageCorrectionParams &imageCorr: imageCorrList) {
            Key imageCorrKey(keyStack, imageCorr.enabled ? lit("_enh") : QString());
            parameters.imageCorrectionParams = imageCorr;

            for (const ar_type &ar: ars) {
                Key arKey(keyStack, ar.first);
                parameters.customAspectRatio = ar.second;

                for(int rotation: rotations) {
                    Key rotKey(keyStack, lit("_rot%1").arg(rotation));
                    parameters.rotationAngle = rotation;

                    for (const crn_type &crn: tsCorners) {
                        Key tsKey(keyStack, crn.first);
                        parameters.timestampPosition = crn.second;

                        for (const QString &fmt: formats) {
                            Key fmtKey(keyStack, fmt);
                            parameters.filename = path();

                            {
                                dialog->setValue(current);
                                current++;
                                dialog->setLabelText(lit("%1 (%2 of %3)").arg(parameters.filename).arg(current).arg(count));
                                qApp->processEvents();
                                if (dialog->wasCanceled())
                                    return;
                                takeScreenshot(widget, parameters);
                                
                            }
                        }
                    }
                }
            }
        }
    }

    dialog->hide();
    qint64 endTime = QDateTime::currentMSecsSinceEpoch();
    QMessageBox::information(mainWindow(), lit("Success"), lit("%1 screenshots done for %2 seconds").arg(count).arg((endTime - startTime) / 1000));
}


void QnWorkbenchScreenshotHandler::at_takeScreenshotAction_triggered() {
    QnActionParameters actionParameters = menu()->currentParameters(sender());
    QString filename = actionParameters.argument<QString>(Qn::FileNameRole);

    QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget *>(actionParameters.widget());
    if (!widget)
        return;

    QnResourceDisplayPtr display = widget->display();
    if (display->videoLayout()->channelCount() == 0) {
        qnWarning("No channels in resource '%1' of type '%2'.", widget->resource()->toResource()->getName(), widget->resource()->toResource()->metaObject()->className());
        return;
    }

    if (qnRuntime->isDevMode() && filename == lit("_DEBUG_SCREENSHOT_KEY_")) {
        takeDebugScreenshotsSet(widget);
        return;
    }

    QnScreenshotParameters parameters;
    parameters.time = screenshotTime(widget);
    parameters.isUtc = widget->resource()->toResource()->flags() & Qn::utc;
    parameters.adjustedTime = screenshotTime(widget, true);
    parameters.filename = filename;
    parameters.timestampPosition = qnSettings->timestampCorner();
    parameters.itemDewarpingParams = widget->item()->dewarpingParams();
    parameters.mediaDewarpingParams = widget->dewarpingParams();
    parameters.imageCorrectionParams = widget->item()->imageEnhancement();
    parameters.zoomRect = parameters.itemDewarpingParams.enabled ? QRectF() : widget->zoomRect();
    parameters.customAspectRatio = display->camDisplay()->overridenAspectRatio();
    parameters.rotationAngle = widget->rotation();

    takeScreenshot(widget, parameters);
}

bool QnWorkbenchScreenshotHandler::updateParametersFromDialog(QnScreenshotParameters &parameters) {
    QString previousDir = qnSettings->lastScreenshotDir();
    if (previousDir.isEmpty())
        previousDir = qnSettings->mediaFolder();
    QString suggestion = replaceNonFileNameCharacters(parameters.filename, QLatin1Char('_'))
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
    setHelpTopic(dialog.data(), Qn::MainWindow_MediaItem_Screenshot_Help);

    QString fileName;

    forever {
        if (!dialog->exec())
            return false;

        /* Check if we were disconnected (server shut down) while the dialog was open.
        * Skip this check if we were not logged in before. */
        if (wasLoggedIn && !context()->user())
            return false;

        fileName = dialog->selectedFile();
        if (fileName.isEmpty())
            return false;

        QString selectedExtension = dialog->selectedExtension();

        if (!fileName.toLower().endsWith(selectedExtension)) {
            fileName += selectedExtension;

            if (QFile::exists(fileName)) {
                QMessageBox::StandardButton button = QMessageBox::information(
                    mainWindow(),
                    tr("Save As"),
                    tr("File '%1' already exists. Do you want to overwrite it?").arg(QFileInfo(fileName).fileName()),
                    QMessageBox::Yes | QMessageBox::No
                    );
                if (button == QMessageBox::No)
                    continue;
            }
        }

        /* Check if we were disconnected (server shut down) while the dialog was open.
        * Skip this check if we were not logged in before. */
        if (wasLoggedIn && !context()->user())
            return false;

        if (QFile::exists(fileName) && !QFile::remove(fileName)) {
            QMessageBox::critical(
                mainWindow(),
                tr("Could not overwrite file"),
                tr("File '%1' is used by another process. Please enter another name.").arg(QFileInfo(fileName).fileName()),
                QMessageBox::Ok
                );
            continue;
        }

        /* Check if we were disconnected (server shut down) while the dialog was open.
        * Skip this check if we were not logged in before. */
        if (wasLoggedIn && !context()->user())
            return false; //TODO: #GDM triple check...

        break;
    }

    parameters.filename = fileName;
    parameters.timestampPosition = static_cast<Qn::Corner>(comboBox->itemData(comboBox->currentIndex()).value<int>());
    return true;
}


//TODO: #GDM #Business may be we should encapsulate in some other object to get parameters and clear connection if user cancelled the process?
void QnWorkbenchScreenshotHandler::at_imageLoaded(const QImage &image) {
    QnScreenshotLoader* loader = dynamic_cast<QnScreenshotLoader*>(sender());
    if (!loader)
        return;
    loader->deleteLater();

    hideProgressDelayed();

    if (m_canceled) {
        hideProgress();
        return;
    }

    QnScreenshotParameters parameters = loader->parameters();
    QImage result = image;

    if (!result.isNull()) {
        qint64 timeMsec = parameters.time == latestScreenshotTime 
            ? QDateTime::currentMSecsSinceEpoch()
            : parameters.adjustedTime / 1000;

        QnImageFilterHelper transcodeParams;
        // Doing heavy filters only. This filters doesn't supported on server side for screenshots
        transcodeParams.setDewarpingParams(parameters.mediaDewarpingParams, parameters.itemDewarpingParams);
        transcodeParams.setContrastParams(parameters.imageCorrectionParams);
        transcodeParams.setTimeCorner(parameters.timestampPosition, 0, timeMsec);
        transcodeParams.setRotation(parameters.rotationAngle);
        transcodeParams.setSrcRect(parameters.zoomRect);
        QList<QnAbstractImageFilterPtr> filters = transcodeParams.createFilterChain(result.size());

        if (!filters.isEmpty()) {
            QSharedPointer<CLVideoDecoderOutput> frame(new CLVideoDecoderOutput(result));
            frame->pts = parameters.time;
            for(auto filter: filters)
                frame = filter->updateImage(frame);
            result = frame->toImage();
        }
    }

    QString filename = parameters.filename;

    if (result.isNull() || !result.save(filename)) {
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

void QnWorkbenchScreenshotHandler::takeScreenshot(QnMediaResourceWidget *widget, const QnScreenshotParameters &parameters) {
    QnScreenshotParameters localParameters = parameters;

    // Revert UI has 'hack'. VerticalDown fisheye option is emulated by additional rotation. But filter has built-in support for that option.
    if (localParameters.itemDewarpingParams.enabled && localParameters.mediaDewarpingParams.viewMode == QnMediaDewarpingParams::VerticalDown)
        localParameters.rotationAngle -= 180;

    QnResourceDisplayPtr display = widget->display();
    QnImageProvider* imageProvider = getLocalScreenshotProvider(widget, localParameters);
    if (imageProvider) {
        // avoiding post-processing duplication
        localParameters.imageCorrectionParams.enabled = false;
        localParameters.mediaDewarpingParams.enabled = false;
        localParameters.itemDewarpingParams.enabled = false;
        localParameters.zoomRect = QRectF();
        localParameters.customAspectRatio = 0.0;
        localParameters.rotationAngle = 0;
    } else {
        QnVirtualCameraResourcePtr camera = widget->resource()->toResourcePtr().dynamicCast<QnVirtualCameraResource>();
        imageProvider = QnSingleThumbnailLoader::newInstance(camera, localParameters.time, 0);
    }

    QnScreenshotLoader* loader = new QnScreenshotLoader(localParameters, this);
    connect(loader, &QnImageProvider::imageChanged, this,   &QnWorkbenchScreenshotHandler::at_imageLoaded);
    loader->setBaseProvider(imageProvider); // preload screenshot here

    /* Check if name is already given - that usually means silent mode. */
    if(localParameters.filename.isEmpty()) {
        localParameters.filename = widget->resource()->toResource()->getName();  /*< suggested name */
        if (!updateParametersFromDialog(localParameters))
            return;
        loader->setParameters(localParameters); //update changed fields
        qnSettings->setLastScreenshotDir(QFileInfo(localParameters.filename).absolutePath());
        qnSettings->setTimestampCorner(localParameters.timestampPosition);

        showProgressDelayed(tr("Saving %1").arg(QFileInfo(localParameters.filename).fileName()));
        connect(m_screenshotProgressDialog, &QnProgressDialog::canceled, loader, &QnScreenshotLoader::deleteLater);
    }

    m_canceled = false;
    loader->loadAsync();
}
