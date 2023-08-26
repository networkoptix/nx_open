// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_screenshot_handler.h"

#include <chrono>

#include <QtCore/QStack>
#include <QtCore/QTimer>
#include <QtCore/qstring.h>
#include <QtGui/QAction>
#include <QtGui/QImageWriter>
#include <QtGui/QPainter>
#include <QtWidgets/QComboBox>

#include <camera/cam_display.h>
#include <camera/resource_display.h>
#include <client/client_runtime_settings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/file_processor.h>
#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/core/transcoding/filters/legacy_transcoding_settings.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/dialogs/progress_dialog.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/image_providers/camera_thumbnail_provider.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/time/formatter.h>
#include <platform/environment.h>
#include <transcoding/filters/filter_helper.h>
#include <ui/dialogs/common/file_messages.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;
using namespace nx::vms::api;

namespace {
    const int showProgressDelay = 1500; // 1.5 sec
    const int minProgressDisplayTime = 1000; // 1 sec

    /* Parameter value to load latest available screenshot. */
    const qint64 latestScreenshotTime = -1;
}

bool SharedScreenshotParameters::isInitialized() const
{
    return !filename.isEmpty();
}

QnScreenshotParameters::QnScreenshotParameters()
{
    sharedParameters.timestampParams.filterParams.enabled = true;
    sharedParameters.timestampParams.filterParams.corner = Qt::BottomRightCorner;
    sharedParameters.cameraNameParams.enabled = true;
    sharedParameters.cameraNameParams.corner = Qt::BottomRightCorner;
}

QString QnScreenshotParameters::timeString(bool forFilename) const
{
    nx::vms::time::Format timeFormat = forFilename
        ? nx::vms::time::Format::filename_time
        : nx::vms::time::Format::hh_mm_ss;
    nx::vms::time::Format fullFormat = forFilename
        ? nx::vms::time::Format::filename_date
        : nx::vms::time::Format::dd_MM_yyyy_hh_mm_ss;

    if (utcTimestampMsec == latestScreenshotTime)
        return nx::vms::time::toString(QTime::currentTime(), timeFormat);
    if (isUtc)
        return nx::vms::time::toString(displayTimeMsec, fullFormat);
    return nx::vms::time::toString(displayTimeMsec, timeFormat);
}

template<typename CornerData>
QComboBox* createCornerComboBox(const QString& emptySettingName,
    const CornerData& data,
    const CornerData& lastSessionData,
    QDialog* parent)
{
    const auto comboBox = new QComboBox(parent);
    comboBox->addItem(emptySettingName, -1);
    comboBox->addItem(QnScreenshotLoader::tr("Top left corner"), Qt::TopLeftCorner);
    comboBox->addItem(QnScreenshotLoader::tr("Top right corner"), Qt::TopRightCorner);
    comboBox->addItem(QnScreenshotLoader::tr("Bottom left corner"), Qt::BottomLeftCorner);
    comboBox->addItem(QnScreenshotLoader::tr("Bottom right corner"), Qt::BottomRightCorner);

    int itemIndex = 0;
    if (appContext()->localSettings()->lastScreenshotParams().isInitialized())
    {
        // Check the selected options from the last session. If `data` is disabled, it means
        // that the option with the index 0 was used.
        itemIndex = lastSessionData.enabled
            ? comboBox->findData(lastSessionData.corner, Qt::UserRole, Qt::MatchExactly)
            : 0;
    }
    else if (data.enabled)
    {
        itemIndex = comboBox->findData(data.corner, Qt::UserRole, Qt::MatchExactly);
    }
    comboBox->setCurrentIndex(itemIndex);
    return comboBox;
}

template <typename CornerData>
void updateCornerData(
    QComboBox* comboBox,
    CornerData& data)
{
    const int index = comboBox->currentIndex();
    const int value = comboBox->itemData(index).toInt();
    data.enabled = (value >= 0);
    if (data.enabled)
        data.corner = static_cast<Qt::Corner>(value);
}

QString getCameraName(const QnMediaResourceWidget* widget)
{
    if (!widget)
        return QString();

    const auto mediaResource = widget->resource();
    if (!mediaResource)
        return QString();

    if (const auto resource = mediaResource->toResource())
        return resource->getName();

    return QString();
}

// -------------------------------------------------------------------------- //
// QnScreenshotLoader
// -------------------------------------------------------------------------- //
QnScreenshotLoader::QnScreenshotLoader(const QnScreenshotParameters& parameters, QObject *parent):
    ImageProvider(parent),
    m_parameters(parameters),
    m_isReady(false)
{
}

QnScreenshotLoader::~QnScreenshotLoader()
{
}

void QnScreenshotLoader::setBaseProvider(ImageProvider *imageProvider)
{
    m_baseProvider.reset(imageProvider);
    if (!imageProvider)
        return;

    connect(imageProvider, &ImageProvider::imageChanged, this, &QnScreenshotLoader::at_imageLoaded);
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
    QnWorkbenchContextAware(parent)
{
    connect(action(action::TakeScreenshotAction), &QAction::triggered,
        this, &QnWorkbenchScreenshotHandler::at_takeScreenshotAction_triggered);
}

ImageProvider* QnWorkbenchScreenshotHandler::getLocalScreenshotProvider(QnMediaResourceWidget *widget,
    const QnScreenshotParameters &parameters, bool forced) const
{
    QnResourceDisplayPtr display = widget->display();

    QnConstResourceVideoLayoutPtr layout = display->videoLayout();
    bool anyQuality = forced || layout->channelCount() > 1;   // screenshots for panoramic cameras will be done locally

    const QnMediaServerResourcePtr server = display->resource()->getParentResource().dynamicCast<QnMediaServerResource>();
    if (!server || server->getServerFlags().testFlag(nx::vms::api::SF_Edge))
        anyQuality = true; // local file or edge cameras will be done locally

    // Either tiling (pano cameras) and crop rect are handled here, so it isn't passed to image processing params

    QnLegacyTranscodingSettings imageProcessingParams;
    imageProcessingParams.resource = display->mediaResource();
    imageProcessingParams.zoomWindow = parameters.zoomRect;
    imageProcessingParams.contrastParams = parameters.imageCorrectionParams;
    imageProcessingParams.itemDewarpingParams = parameters.itemDewarpingParams;
    imageProcessingParams.rotation = parameters.rotationAngle;
    imageProcessingParams.forcedAspectRatio = parameters.customAspectRatio;
    imageProcessingParams.watermark = watermark();

    QImage screenshot = display->camDisplay()->getScreenshot(imageProcessingParams, anyQuality);
    if (screenshot.isNull())
        return nullptr;

    return new BasicImageProvider(screenshot);
}

qint64 QnWorkbenchScreenshotHandler::screenshotTimeMSec(QnMediaResourceWidget *widget, bool adjust) {
    QnResourceDisplayPtr display = widget->display();

    qint64 timeUsec = display->camDisplay()->getCurrentTime();
    if (timeUsec == qint64(AV_NOPTS_VALUE))
        return latestScreenshotTime;

    qint64 timeMSec = timeUsec / 1000;
    if (!adjust)
        return timeMSec;

    const auto timeWatcher = widget->systemContext()->serverTimeWatcher();
    qint64 localOffset = timeWatcher->displayOffset(widget->resource());

    timeMSec += localOffset;
    return timeMSec;
}

void QnWorkbenchScreenshotHandler::takeDebugScreenshotsSet(QnMediaResourceWidget *widget) {
    QStack<QString> keyStack;
    SharedScreenshotParameters lastParams = appContext()->localSettings()->lastScreenshotParams();
    QString previousDir =
        lastParams.isInitialized() ? QFileInfo(lastParams.filename).absolutePath() : QString();
    if (!previousDir.isEmpty())
        keyStack.push(previousDir + "/");

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

    typedef QPair<QString, QnAspectRatio> ar_type;
    QList<ar_type> ars;
    ars << ar_type(QString(), QnAspectRatio());
    for (const QnAspectRatio &ar: QnAspectRatio::standardRatios())
        ars << ar_type(ar.toString(lit("_%1x%2")), ar);
    count*= ars.size();

    QList<int> rotations;
    for(int rotation = 0; rotation < 360; rotation += 90)
        rotations << rotation;
    count *= rotations.size();

    QList<nx::vms::api::ImageCorrectionData> imageCorrList;
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

    QScopedPointer<ProgressDialog> dialog(new ProgressDialog(mainWindowWidget()));
    dialog->setRange(0, count);
    dialog->setValue(current);
    dialog->setMinimumWidth(600);
    dialog->show();

    qint64 startTime = QDateTime::currentMSecsSinceEpoch();

    QnScreenshotParameters parameters;
    {
        parameters.utcTimestampMsec = screenshotTimeMSec(widget);
        parameters.isUtc = widget->resource()->toResource()->flags() & Qn::utc;
        parameters.displayTimeMsec = screenshotTimeMSec(widget, true);
        Key timeKey(keyStack, lit("_") + parameters.timeString(/*forFilename*/ true));

        parameters.itemDewarpingParams = widget->item()->dewarpingParams();
        parameters.resource = widget->resource();
        parameters.zoomRect = parameters.itemDewarpingParams.enabled ? QRectF() : widget->zoomRect();

        for (const auto& imageCorr: imageCorrList)
        {
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

                        auto& filterParams = parameters.sharedParameters.timestampParams.filterParams;
                        filterParams.enabled = crn.second >= 0;
                        if (filterParams.enabled)
                            filterParams.corner = static_cast<Qt::Corner>(crn.second);

                        for (const QString &fmt: formats) {
                            Key fmtKey(keyStack, fmt);
                            parameters.sharedParameters.filename = path();

                            {
                                dialog->setValue(current);
                                current++;
                                dialog->setText(nx::format("%1 (%2 of %3)",
                                    parameters.sharedParameters.filename,
                                    current,
                                    count));
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
    QnMessageBox::success(mainWindowWidget(),
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
        NX_ASSERT(false, "No channels in resource '%1' of type '%2'.", widget->resource()->toResource()->getName(), widget->resource()->toResource()->metaObject()->className());
        return;
    }

    if (ini().developerMode && filename == lit("_DEBUG_SCREENSHOT_KEY_")) {
        takeDebugScreenshotsSet(widget);
        return;
    }

    QnScreenshotParameters parameters;
    parameters.utcTimestampMsec = screenshotTimeMSec(widget);
    parameters.isUtc = widget->resource()->toResource()->flags() & Qn::utc;
    parameters.displayTimeMsec = screenshotTimeMSec(widget, true);
    parameters.sharedParameters.filename = filename;
    // TODO: #sivanov Store full screenshot settings.
    parameters.itemDewarpingParams = widget->item()->dewarpingParams();
    parameters.resource = widget->resource();
    parameters.imageCorrectionParams = widget->item()->imageEnhancement();
    parameters.zoomRect = parameters.itemDewarpingParams.enabled ? QRectF() : widget->zoomRect();
    parameters.customAspectRatio = display->camDisplay()->overridenAspectRatio();
    parameters.rotationAngle = widget->rotation();
    takeScreenshot(widget, parameters);
}

std::vector<QnCustomFileDialog::FileFilter> QnWorkbenchScreenshotHandler::generateFileFilters()
{
    const QnCustomFileDialog::FileFilter pngFilter{tr("PNG Image"), "png"};

    std::vector<QnCustomFileDialog::FileFilter> allowedFilters{pngFilter};
    if (QImageWriter::supportedImageFormats().contains("jpg"))
    {
        const QnCustomFileDialog::FileFilter jpgFilter{tr("JPEG Image"), {"jpg", "jpeg"}};

        // Depending on the latest screenshot parameters put the JPEG filter to the begining or
        // the end of the filter list.
        if (QFileInfo(appContext()->localSettings()->lastScreenshotParams().filename)
            .fileName().endsWith("png"))
        {
            allowedFilters.push_back(jpgFilter);
        }
        else
        {
            allowedFilters.insert(allowedFilters.begin(), jpgFilter);
        }
    }

    return allowedFilters;
}

bool QnWorkbenchScreenshotHandler::updateParametersFromDialog(QnScreenshotParameters& parameters)
{
    QString previousDir;
    bool useLastScreenshotParams =
        appContext()->localSettings()->lastScreenshotParams().isInitialized();
    if (useLastScreenshotParams)
    {
        previousDir = QFileInfo(appContext()->localSettings()->lastScreenshotParams().filename)
            .absolutePath();
    }
    else if (!appContext()->localSettings()->mediaFolders().isEmpty())
    {
        previousDir = appContext()->localSettings()->mediaFolders().first();
    }

    static constexpr int kMaxFileNameLength = 200;
    const auto baseFileName = parameters.sharedParameters.filename.length() > kMaxFileNameLength
        ? parameters.sharedParameters.filename.left(kMaxFileNameLength) + '~'
        : parameters.sharedParameters.filename;
    QString suggestion = nx::utils::replaceNonFileNameCharacters(baseFileName
        + QLatin1Char('_') + parameters.timeString(true), QLatin1Char('_')).
        replace(QChar::Space, QLatin1Char('_'));
    suggestion = QnEnvironment::getUniqueFileName(previousDir, suggestion);

    bool wasLoggedIn = !context()->user().isNull();

    QScopedPointer<QnSessionAwareFileDialog> dialog(
        new QnSessionAwareFileDialog (
        mainWindowWidget(),
        tr("Save Screenshot As..."),
        suggestion,
        QnCustomFileDialog::createFilter(generateFileFilters())));

    dialog->setFileMode(QFileDialog::AnyFile);
    dialog->setAcceptMode(QFileDialog::AcceptSave);

    const auto lastScreenshotParameters = appContext()->localSettings()->lastScreenshotParams();
    const auto timestampComboBox = createCornerComboBox(tr("No timestamp"),
        parameters.sharedParameters.timestampParams.filterParams,
        lastScreenshotParameters.timestampParams.filterParams,
        dialog.data());

    const auto cameraNameComboBox = createCornerComboBox(tr("No camera name"),
        parameters.sharedParameters.cameraNameParams,
        lastScreenshotParameters.cameraNameParams,
        dialog.data());

    dialog->addWidget(tr("Timestamp:"), timestampComboBox);
    dialog->addWidget(tr("Camera name:"), cameraNameComboBox);
    setAccentStyle(dialog.data(), QDialogButtonBox::Save);
    setHelpTopic(dialog.data(), HelpTopic::Id::MainWindow_MediaItem_Screenshot);

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
                    mainWindowWidget(), QFileInfo(fileName).fileName()))
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
                mainWindowWidget(), QFileInfo(fileName).fileName());
            continue;
        }

        /* Check if we were disconnected (server shut down) while the dialog was open.
        * Skip this check if we were not logged in before. */
        if (wasLoggedIn && !context()->user())
            return false; // TODO: #sivanov Triple check.

        break;
    }

    updateCornerData(timestampComboBox, parameters.sharedParameters.timestampParams.filterParams);
    updateCornerData(cameraNameComboBox, parameters.sharedParameters.cameraNameParams);
    parameters.sharedParameters.filename = fileName;
    return true;
}

// TODO: #sivanov May be we should encapsulate in some other object to get parameters and clear
// connection if user cancelled the process?
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
        // TODO: #sivanov Simplify the logic.
        parameters.sharedParameters.timestampParams.timeMs = parameters.utcTimestampMsec == latestScreenshotTime
            ? QDateTime::currentMSecsSinceEpoch()
            : parameters.displayTimeMsec;

        QnLegacyTranscodingSettings transcodeParams;
        // Doing heavy filters only. This filters doesn't supported on server side for screenshots
        transcodeParams.itemDewarpingParams = parameters.itemDewarpingParams;
        transcodeParams.resource = parameters.resource;
        transcodeParams.contrastParams = parameters.imageCorrectionParams;
        transcodeParams.timestampParams = parameters.sharedParameters.timestampParams;
        transcodeParams.cameraNameParams = parameters.sharedParameters.cameraNameParams;
        transcodeParams.rotation = parameters.rotationAngle;
        transcodeParams.zoomWindow = parameters.zoomRect;
        transcodeParams.watermark = watermark();
        auto filters = QnImageFilterHelper::createFilterChain(
            transcodeParams);

        // Thumbnail from loader is already merged for panoramic cameras.
        const QSize noDownscale(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
        filters.prepareForImage(result.size(), noDownscale);

        if (!filters.isEmpty())
        {
            QSharedPointer<CLVideoDecoderOutput> frame(new CLVideoDecoderOutput(result));
            // TODO: #sivanov How is this supposed to work with latestScreenshotTime?
            frame->pts = parameters.utcTimestampMsec * 1000;
            frame = filters.apply(frame);
            result = frame ? frame->toImage() : QImage();
        }
    }

    QString filename = parameters.sharedParameters.filename;

    if (result.isNull() || !result.save(filename))
    {
        hideProgress();

        QnMessageBox::critical(mainWindowWidget(), tr("Failed to save screenshot"));
        return;
    }

    if (auto existing = QnFileProcessor::findResourceForFile(filename, resourcePool()))
        resourcePool()->removeResource(existing);

    QnFileProcessor::createResourcesForFile(filename, resourcePool());
}

void QnWorkbenchScreenshotHandler::showProgressDelayed(const QString &message) {
    if (!m_screenshotProgressDialog) {
        m_screenshotProgressDialog = new QnSessionAware<ProgressDialog>(mainWindowWidget());
        m_screenshotProgressDialog->setWindowTitle(tr("Saving Screenshot..."));
        m_screenshotProgressDialog->setInfiniteMode();
        // TODO: #dklychkov ensure concurrent screenshot saving is ok and disable modality
        m_screenshotProgressDialog->setModal(true);
        connect(m_screenshotProgressDialog, &ProgressDialog::canceled, this,
            &QnWorkbenchScreenshotHandler::cancelLoading);
    }

    m_screenshotProgressDialog->setText(message);

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

    // Revert UI has specifics. `VerticalDown` fisheye option is emulated by additional rotation.
    // But filter has built-in support for that option.
    if (localParameters.itemDewarpingParams.enabled)
    {
        const auto params = widget->resource()->getDewarpingParams();
        if (params.isFisheye() && params.viewMode == dewarping::FisheyeCameraMount::ceiling)
            localParameters.rotationAngle -= 180;
    }

    QnResourceDisplayPtr display = widget->display();
    ImageProvider* imageProvider = getLocalScreenshotProvider(widget, localParameters);
    if (imageProvider) {
        // avoiding post-processing duplication
        localParameters.imageCorrectionParams.enabled = false;
        localParameters.itemDewarpingParams.enabled = false;
        localParameters.zoomRect = QRectF();
        localParameters.customAspectRatio = QnAspectRatio();
        localParameters.rotationAngle = 0;
    } else {
        QnVirtualCameraResourcePtr camera = widget->resource()->toResourcePtr().dynamicCast<QnVirtualCameraResource>();
        NX_ASSERT(camera, "Camera must exist here");
        if (camera)
        {
            nx::api::CameraImageRequest request;
            request.camera = camera;
            request.roundMethod = nx::api::ImageRequest::RoundMethod::precise;
            request.rotation = 0;

            // TODO: #vkutin Change to `raw` when it's supported.
            request.format = nx::api::ImageRequest::ThumbnailFormat::png;

            request.streamSelectionMode =
                nx::api::CameraImageRequest::StreamSelectionMode::forcedPrimary;

            request.timestampMs = localParameters.utcTimestampMsec == latestScreenshotTime
                ? nx::api::ImageRequest::kLatestThumbnail
                : std::chrono::milliseconds(localParameters.utcTimestampMsec);

            imageProvider = new nx::vms::client::desktop::CameraThumbnailProvider(request);
        }
        else
            imageProvider = getLocalScreenshotProvider(widget, localParameters, true);
    }

    if (!imageProvider)
    {
        QnMessageBox::critical(mainWindowWidget(), tr("Failed to take screenshot"));
        return;
    }

    QScopedPointer<QnScreenshotLoader> loader(new QnScreenshotLoader(localParameters, this));
    connect(loader.get(), &ImageProvider::imageChanged, this,   &QnWorkbenchScreenshotHandler::at_imageLoaded);
    loader->setBaseProvider(imageProvider); // preload screenshot here

    /* Check if name is already given - that usually means silent mode. */
    if (localParameters.sharedParameters.filename.isEmpty())
    {
        localParameters.sharedParameters.filename =
            widget->resource()->toResource()->getName(); //< Suggested name.
        if (!updateParametersFromDialog(localParameters))
            return;

        loader->setParameters(localParameters); //< Update changed fields.

        appContext()->localSettings()->lastScreenshotParams = localParameters.sharedParameters;

        showProgressDelayed(
            tr("Saving %1").arg(QFileInfo(localParameters.sharedParameters.filename).fileName()));
        connect(m_screenshotProgressDialog,
            &ProgressDialog::canceled,
            loader.get(),
            &QnScreenshotLoader::deleteLater);
    }
    m_canceled = false;
    loader.take()->loadAsync(); //< Remove owning
}
