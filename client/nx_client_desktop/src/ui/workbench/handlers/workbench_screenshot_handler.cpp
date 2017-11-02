#include "workbench_screenshot_handler.h"

#include <QtCore/QTimer>
#include <QtCore/QStack>

#include <QtGui/QImageWriter>
#include <QtGui/QPainter>

#include <QtWidgets/QAction>
#include <QtWidgets/QComboBox>

#include <camera/cam_display.h>
#include <camera/resource_display.h>
#include <camera/single_thumbnail_loader.h>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <core/resource/file_processor.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <platform/environment.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/progress_dialog.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/dialogs/common/file_messages.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>

#include <nx/utils/string.h>
#include <utils/common/warnings.h>

#include "transcoding/filters/filter_helper.h"
#include <nx/core/transcoding/filters/legacy_transcoding_settings.h>

using namespace nx::client::desktop::ui;

namespace {
    const int showProgressDelay = 1500; // 1.5 sec
    const int minProgressDisplayTime = 1000; // 1 sec

    /* Parameter value to load latest available screenshot. */
    const qint64 latestScreenshotTime = -1;
}

QnScreenshotParameters::QnScreenshotParameters()
{
    timestampParams.enabled = true;
    timestampParams.corner = Qt::BottomRightCorner;
}

QString QnScreenshotParameters::timeString() const {
    if (utcTimestampMsec == latestScreenshotTime)
        return QDateTime::currentDateTime().toString(lit("hh.mm.ss"));

    qint64 timeMSecs = displayTimeMsec;
    if (isUtc)
        return nx::utils::datetimeSaveDialogSuggestion(QDateTime::fromMSecsSinceEpoch(timeMSecs));
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

QnScreenshotLoader::~QnScreenshotLoader()
{
}

void QnScreenshotLoader::setBaseProvider(QnImageProvider *imageProvider)
{
    m_baseProvider.reset(imageProvider);
    if (!imageProvider)
        return;

    connect(imageProvider, &QnImageProvider::imageChanged, this, &QnScreenshotLoader::at_imageLoaded);
    imageProvider->loadAsync();
}

QImage QnScreenshotLoader::image() const
{
    if (!m_baseProvider)
        return QImage();
    return m_baseProvider->image();
}

QSize QnScreenshotLoader::sizeHint() const
{
    if (!m_baseProvider)
        return QSize();
    return m_baseProvider->sizeHint();
}

Qn::ThumbnailStatus QnScreenshotLoader::status() const
{
    if (!m_baseProvider)
        return Qn::ThumbnailStatus::Invalid;
    return m_baseProvider->status();
}

QnScreenshotParameters QnScreenshotLoader::parameters() const
{
    return m_parameters;
}

void QnScreenshotLoader::setParameters(const QnScreenshotParameters &parameters)
{
    m_parameters = parameters;
}

void QnScreenshotLoader::doLoadAsync()
{
    m_isReady = true;

    QImage img = image();
    if (img.isNull())
        return; //waiting for image to be loaded

    // image is already loaded within base provider
    emit imageChanged(img);
    m_isReady = false;
}

void QnScreenshotLoader::at_imageLoaded(const QImage &image)
{
    if (!m_isReady)
        return;
    emit imageChanged(image);
    m_isReady = false;
}

// -------------------------------------------------------------------------- //
// QnWorkbenchScreenshotHandler
// -------------------------------------------------------------------------- //
QnWorkbenchScreenshotHandler::QnWorkbenchScreenshotHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_screenshotProgressDialog(0),
    m_progressShowTime(0),
    m_canceled(false)
{
    connect(action(action::TakeScreenshotAction), &QAction::triggered, this, &QnWorkbenchScreenshotHandler::at_takeScreenshotAction_triggered);
}

QnImageProvider* QnWorkbenchScreenshotHandler::getLocalScreenshotProvider(QnMediaResourceWidget *widget, const QnScreenshotParameters &parameters, bool forced) const {
    QnResourceDisplayPtr display = widget->display();

    QnConstResourceVideoLayoutPtr layout = display->videoLayout();
    bool anyQuality = forced || layout->channelCount() > 1;   // screenshots for panoramic cameras will be done locally

    const QnMediaServerResourcePtr server = display->resource()->getParentResource().dynamicCast<QnMediaServerResource>();
    if (!server || (server->getServerFlags() & Qn::SF_Edge))
        anyQuality = true; // local file or edge cameras will be done locally

    // Either tiling (pano cameras) and crop rect are handled here, so it isn't passed to image processing params

    QnLegacyTranscodingSettings imageProcessingParams;
    imageProcessingParams.resource = display->mediaResource();
    imageProcessingParams.zoomWindow = parameters.zoomRect;
    imageProcessingParams.contrastParams = parameters.imageCorrectionParams;
    imageProcessingParams.itemDewarpingParams = parameters.itemDewarpingParams;
    imageProcessingParams.rotation = parameters.rotationAngle;
    imageProcessingParams.forcedAspectRatio = parameters.customAspectRatio;

    QImage screenshot = display->camDisplay()->getScreenshot(imageProcessingParams, anyQuality);
    if (screenshot.isNull())
        return NULL;

    return new QnBasicImageProvider(screenshot);
}

qint64 QnWorkbenchScreenshotHandler::screenshotTimeMSec(QnMediaResourceWidget *widget, bool adjust) {
    QnResourceDisplayPtr display = widget->display();

    qint64 timeUsec = display->camDisplay()->getCurrentTime();
    if (timeUsec == qint64(AV_NOPTS_VALUE))
        return latestScreenshotTime;

    qint64 timeMSec = timeUsec / 1000;
    if (!adjust)
        return timeMSec;

    qint64 localOffset = context()->instance<QnWorkbenchServerTimeWatcher>()->displayOffset(widget->resource());

    timeMSec += localOffset;
    return timeMSec;
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

    typedef QPair<QString, int> crn_type;
    QList<crn_type> tsCorners;
    tsCorners
        << crn_type(lit("_nots"), -1)
        << crn_type(lit("_topLeft"), Qt::TopLeftCorner)
        << crn_type(lit("_topRight"), Qt::TopRightCorner)
        << crn_type(lit("_btmLeft"), Qt::BottomLeftCorner)
        << crn_type(lit("_btmRight"), Qt::BottomRightCorner);
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
        parameters.utcTimestampMsec = screenshotTimeMSec(widget);
        parameters.isUtc = widget->resource()->toResource()->flags() & Qn::utc;
        parameters.displayTimeMsec = screenshotTimeMSec(widget, true);
        Key timeKey(keyStack, lit("_") + parameters.timeString());

        parameters.itemDewarpingParams = widget->item()->dewarpingParams();
        parameters.resource = widget->resource();
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

                        parameters.timestampParams.enabled = crn.second >= 0;
                        if (parameters.timestampParams.enabled)
                            parameters.timestampParams.corner = static_cast<Qt::Corner>(crn.second);

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
    QnMessageBox::success(mainWindow(),
        lit("%1 screenshots done for %2 seconds").arg(count).arg((endTime - startTime) / 1000));
}


void QnWorkbenchScreenshotHandler::at_takeScreenshotAction_triggered() {
    const auto actionParameters = menu()->currentParameters(sender());
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
    parameters.utcTimestampMsec = screenshotTimeMSec(widget);
    parameters.isUtc = widget->resource()->toResource()->flags() & Qn::utc;
    parameters.displayTimeMsec = screenshotTimeMSec(widget, true);
    parameters.filename = filename;
   // parameters.timestampPosition = qnSettings->timestampCorner(); // TODO: #GDM #3.1 store full screenshot settings
    parameters.itemDewarpingParams = widget->item()->dewarpingParams();
    parameters.resource = widget->resource();
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
    QString suggestion = nx::utils::replaceNonFileNameCharacters(parameters.filename, QLatin1Char('_'))
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

    QScopedPointer<QnSessionAwareFileDialog> dialog(
        new QnSessionAwareFileDialog (
        mainWindow(),
        tr("Save Screenshot As..."),
        suggestion,
        allowedFormatFilter
        ));

    dialog->setFileMode(QFileDialog::AnyFile);
    dialog->setAcceptMode(QFileDialog::AcceptSave);

    QComboBox* comboBox = new QComboBox(dialog.data());
    comboBox->addItem(tr("No Timestamp"), -1);
    comboBox->addItem(tr("Top Left Corner"), Qt::TopLeftCorner);
    comboBox->addItem(tr("Top Right Corner"), Qt::TopRightCorner);
    comboBox->addItem(tr("Bottom Left Corner"), Qt::BottomLeftCorner);
    comboBox->addItem(tr("Bottom Right Corner"), Qt::BottomRightCorner);

    if (!parameters.timestampParams.enabled)
        comboBox->setCurrentIndex(0);
    else
        comboBox->setCurrentIndex(comboBox->findData(parameters.timestampParams.corner, Qt::UserRole, Qt::MatchExactly));

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

            if (QFile::exists(fileName)
                && !QnFileMessages::confirmOverwrite(
                    mainWindow(), QFileInfo(fileName).fileName()))
            {
                continue;
            }
        }

        /* Check if we were disconnected (server shut down) while the dialog was open.
        * Skip this check if we were not logged in before. */
        if (wasLoggedIn && !context()->user())
            return false;

        if (QFile::exists(fileName) && !QFile::remove(fileName))
        {
            QnFileMessages::overwriteFailed(
                mainWindow(), QFileInfo(fileName).fileName());
            continue;
        }

        /* Check if we were disconnected (server shut down) while the dialog was open.
        * Skip this check if we were not logged in before. */
        if (wasLoggedIn && !context()->user())
            return false; // TODO: #GDM triple check...

        break;
    }

    {
        int corner = comboBox->itemData(comboBox->currentIndex()).toInt();
        parameters.timestampParams.enabled = (corner >= 0);
        if (parameters.timestampParams.enabled)
            parameters.timestampParams.corner = static_cast<Qt::Corner>(corner);
    }

    parameters.filename = fileName;
    return true;
}


// TODO: #GDM #Business may be we should encapsulate in some other object to get parameters and clear connection if user cancelled the process?
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
        // TODO: #GDM #3.2 looks like total mess
        parameters.timestampParams.timeMs = parameters.utcTimestampMsec == latestScreenshotTime
            ? QDateTime::currentMSecsSinceEpoch()
            : parameters.displayTimeMsec;

        QnLegacyTranscodingSettings transcodeParams;
        // Doing heavy filters only. This filters doesn't supported on server side for screenshots
        transcodeParams.itemDewarpingParams = parameters.itemDewarpingParams;
        transcodeParams.resource = parameters.resource;
        transcodeParams.contrastParams = parameters.imageCorrectionParams;
        transcodeParams.timestampParams = parameters.timestampParams;
        transcodeParams.rotation = parameters.rotationAngle;
        transcodeParams.zoomWindow = parameters.zoomRect;
        auto filters = QnImageFilterHelper::createFilterChain(
            transcodeParams);

        // Thumbnail from loader is already merged for panoramic cameras.
        const QSize noDownscale(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
        filters.prepareForImage(parameters.resource, result.size(), noDownscale);

        if (!filters.isEmpty())
        {
            QSharedPointer<CLVideoDecoderOutput> frame(new CLVideoDecoderOutput(result));
            // TODO: #GDM how is this supposed to work with latestScreenshotTime?
            frame->pts = parameters.utcTimestampMsec * 1000;
            frame = filters.apply(frame);
            result = frame ? frame->toImage() : QImage();
        }
    }

    QString filename = parameters.filename;

    if (result.isNull() || !result.save(filename))
    {
        hideProgress();

        QnMessageBox::critical(mainWindow(), tr("Failed to save screenshot"));
        return;
    }

    QnFileProcessor::createResourcesForFile(filename);
}

void QnWorkbenchScreenshotHandler::showProgressDelayed(const QString &message) {
    if (!m_screenshotProgressDialog) {
        m_screenshotProgressDialog = new QnSessionAware<QnProgressDialog>(mainWindow());
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
    if (localParameters.itemDewarpingParams.enabled
        && widget->resource()->getDewarpingParams().viewMode == QnMediaDewarpingParams::VerticalDown)
    {
        localParameters.rotationAngle -= 180;
    }

    QnResourceDisplayPtr display = widget->display();
    QnImageProvider* imageProvider = getLocalScreenshotProvider(widget, localParameters);
    if (imageProvider) {
        // avoiding post-processing duplication
        localParameters.imageCorrectionParams.enabled = false;
        localParameters.itemDewarpingParams.enabled = false;
        localParameters.zoomRect = QRectF();
        localParameters.customAspectRatio = 0.0;
        localParameters.rotationAngle = 0;
    } else {
        QnVirtualCameraResourcePtr camera = widget->resource()->toResourcePtr().dynamicCast<QnVirtualCameraResource>();
        NX_ASSERT(camera, Q_FUNC_INFO, "Camera must exist here");
        if (camera)
        {
            auto provider = new QnSingleThumbnailLoader(camera, localParameters.utcTimestampMsec, 0);
            auto params = provider->requestData();
            params.roundMethod = QnThumbnailRequestData::PreciseMethod;
            provider->setRequestData(params);
            imageProvider = provider;
        }
        else
            imageProvider = getLocalScreenshotProvider(widget, localParameters, true);
    }

    if (!imageProvider)
    {
        QnMessageBox::critical(mainWindow(), tr("Failed to take screenshot"));
        return;
    }

    QScopedPointer<QnScreenshotLoader> loader(new QnScreenshotLoader(localParameters, this));
    connect(loader, &QnImageProvider::imageChanged, this,   &QnWorkbenchScreenshotHandler::at_imageLoaded);
    loader->setBaseProvider(imageProvider); // preload screenshot here

    /* Check if name is already given - that usually means silent mode. */
    if(localParameters.filename.isEmpty()) {
        localParameters.filename = widget->resource()->toResource()->getName();  /*< suggested name */
        if (!updateParametersFromDialog(localParameters))
            return;

        loader->setParameters(localParameters); //update changed fields
        qnSettings->setLastScreenshotDir(QFileInfo(localParameters.filename).absolutePath());
        // TODO: #GDM #3.1 store screenshot settings
        //qnSettings->setTimestampCorner(localParameters.timestampPosition);

        showProgressDelayed(tr("Saving %1").arg(QFileInfo(localParameters.filename).fileName()));
        connect(m_screenshotProgressDialog, &QnProgressDialog::canceled, loader, &QnScreenshotLoader::deleteLater);
    }

    m_canceled = false;
    loader.take()->loadAsync(); //< Remove owning
}
