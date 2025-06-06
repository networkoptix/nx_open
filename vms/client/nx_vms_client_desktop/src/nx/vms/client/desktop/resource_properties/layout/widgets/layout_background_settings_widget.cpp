// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_background_settings_widget.h"
#include "ui_layout_background_settings_widget.h"

#include <QtCore/QUrl>
#include <QtCore/QtMath>
#include <QtGui/QDesktopServices>
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include <QtGui/QScreen>
#include <QtWidgets/QApplication>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/image_providers/threaded_image_loader.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/local_file_cache.h>
#include <nx/vms/client/desktop/utils/server_image_cache.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/image_preview_dialog.h>
#include <ui/widgets/common/framed_label.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <utils/common/delayed.h>

#include "../flux/layout_settings_dialog_state.h"
#include "../flux/layout_settings_dialog_state_reducer.h"
#include "../flux/layout_settings_dialog_store.h"

namespace nx::vms::client::desktop {

using State = LayoutSettingsDialogState;
using BackgroundImageStatus = State::BackgroundImageStatus;

namespace {

QString braced(const QString& source)
{
    return '<' + source + '>';
};

static constexpr qreal kOpaque = 1.0;
static constexpr qreal kOpacityPercent = 0.01;

//TODO: #sivanov Code duplication.
// Aspect ratio of the current screen.
QnAspectRatio screenAspectRatio()
{
    const auto screen = qApp->primaryScreen();
    return QnAspectRatio(screen->size());
}

} // namespace

struct LayoutBackgroundSettingsWidget::Private
{
    QPointer<ServerImageCache> cache;

    // Workaround for WhatsThis mode clicks.
    bool skipNextMouseReleaseEvent = false;
};

LayoutBackgroundSettingsWidget::LayoutBackgroundSettingsWidget(
    LayoutSettingsDialogStore* store,
    QWidget* parent) :
    base_type(parent),
    d(new Private()),
    ui(new Ui::LayoutBackgroundSettingsWidget),
    m_store(store)
{
    setupUi();

    LayoutSettingsDialogStateReducer::setScreenAspectRatio(screenAspectRatio());
    LayoutSettingsDialogStateReducer::setKeepBackgroundAspectRatio(
        appContext()->localSettings()->layoutKeepAspectRatio());

    connect(
        ui->viewButton,
        &QPushButton::clicked,
        this,
        &LayoutBackgroundSettingsWidget::viewFile);

    connect(
        ui->selectButton,
        &QPushButton::clicked,
        this,
        &LayoutBackgroundSettingsWidget::selectFile);

    connect(
        ui->clearButton,
        &QPushButton::clicked,
        store,
        &LayoutSettingsDialogStore::clearBackgroundImage);

    connect(
        ui->opacitySpinBox,
        QnSpinboxIntValueChanged,
        store,
        &LayoutSettingsDialogStore::setBackgroundImageOpacityPercent);

    connect(
        ui->widthSpinBox,
        QnSpinboxIntValueChanged,
        store,
        &LayoutSettingsDialogStore::setBackgroundImageWidth);

    connect(
        ui->heightSpinBox,
        QnSpinboxIntValueChanged,
        store,
        &LayoutSettingsDialogStore::setBackgroundImageHeight);

    connect(
        ui->cropToMonitorCheckBox,
        &QCheckBox::clicked,
        store,
        &LayoutSettingsDialogStore::setCropToMonitorAspectRatio);

    connect(
        ui->keepAspectRatioCheckBox,
        &QCheckBox::clicked,
        store,
        &LayoutSettingsDialogStore::setKeepImageAspectRatio);

    connect(
        store,
        &LayoutSettingsDialogStore::stateChanged,
        this,
        &LayoutBackgroundSettingsWidget::loadState);
}

LayoutBackgroundSettingsWidget::~LayoutBackgroundSettingsWidget()
{
}

void LayoutBackgroundSettingsWidget::uploadImage()
{
    const auto& background = m_store->state().background;
    if (background.cropToMonitorAspectRatio)
        d->cache->storeImage(background.imageSourcePath, screenAspectRatio());
    else
        d->cache->storeImage(background.imageSourcePath);
    m_store->startUploading();
}

bool LayoutBackgroundSettingsWidget::eventFilter(QObject* target, QEvent* event)
{
    if (event->type() == QEvent::LeaveWhatsThisMode)
    {
        d->skipNextMouseReleaseEvent = true;
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        if (target == ui->imageLabel && !d->skipNextMouseReleaseEvent)
        {
            const auto& state = m_store->state();

            const bool locked = state.locked;
            const bool canSelectImage = !locked
                && (state.background.status == BackgroundImageStatus::empty
                    || state.background.status == BackgroundImageStatus::error);
            const bool canViewImage = state.background.imagePresent();

            if (canSelectImage)
                selectFile();
            else if (canViewImage)
                viewFile();
        }
        d->skipNextMouseReleaseEvent = false;
    }

    return base_type::eventFilter(target, event);
}

void LayoutBackgroundSettingsWidget::setupUi()
{
    ui->setupUi(this);

    ui->widthSpinBox->setSuffix(' ' + tr("cells"));
    ui->heightSpinBox->setSuffix(' ' + tr("cells"));

    setHelpTopic(this, HelpTopic::Id::LayoutSettings_EMapping);

    installEventFilter(this);
    ui->imageLabel->installEventFilter(this);
    ui->imageLabel->setFrameColor(core::colorTheme()->color("layoutBackgroundSettings.frameColor"));
    ui->imageLabel->setAutoScale(true);
}

void LayoutBackgroundSettingsWidget::initCache(SystemContext* systemContext, bool isLocalFile)
{
    if (d->cache)
        d->cache->disconnect(this);

    d->cache = isLocalFile
        ? systemContext->localFileCache()
        : systemContext->serverImageCache();

    // Cross-site and CSL contexts have no file caches.
    if (!d->cache)
        return;

    connect(
        d->cache,
        &ServerFileCache::fileDownloaded,
        this,
        [this](const QString& filename, ServerFileCache::OperationResult status)
        {
            onImageDownloaded(filename, status == ServerFileCache::OperationResult::ok);
        });

    connect(
        d->cache,
        &ServerFileCache::fileUploaded,
        this,
        [this](const QString& filename, ServerFileCache::OperationResult status)
        {
            onImageUploaded(filename, status == ServerFileCache::OperationResult::ok);
        });
}

void LayoutBackgroundSettingsWidget::loadState(const LayoutSettingsDialogState& state)
{
    const auto& background = state.background;

    if (state.background.canStartDownloading())
        executeLater([this] { startDownloading(); }, this);

    NX_ASSERT(background.width.max <= State::kBackgroundMaxSize);
    ui->widthSpinBox->setRange(background.width.min, background.width.max);
    ui->widthSpinBox->setValue(background.width.value);

    NX_ASSERT(background.height.max <= State::kBackgroundMaxSize);
    ui->heightSpinBox->setRange(background.height.min, background.height.max);
    ui->heightSpinBox->setValue(background.height.value);

    ui->keepAspectRatioCheckBox->setChecked(background.keepImageAspectRatio);
    ui->opacitySpinBox->setValue(background.opacityPercent);

    ui->stackedWidget->setCurrentWidget(background.loadingInProgress()
        ? ui->loadingPage
        : ui->imagePage);

    const bool imagePresent = background.imagePresent();

    ui->widthSpinBox->setEnabled(imagePresent && !state.locked);
    ui->heightSpinBox->setEnabled(imagePresent && !state.locked);
    ui->keepAspectRatioCheckBox->setEnabled(imagePresent && !state.locked);
    ui->viewButton->setEnabled(imagePresent);
    ui->clearButton->setEnabled(imagePresent && !state.locked);
    ui->opacitySpinBox->setEnabled(imagePresent && !state.locked);
    ui->selectButton->setEnabled(!state.locked);

    // Mark image as already cropped if actual.
    ui->cropToMonitorCheckBox->setChecked(background.cropToMonitorAspectRatio
        || !background.canCropImage());

    ui->cropToMonitorCheckBox->setEnabled(
        imagePresent && !state.locked && background.canCropImage());

    if (!imagePresent)
    {
        ui->imageLabel->setPixmap(QPixmap());
        ui->imageLabel->setText(
            background.status != BackgroundImageStatus::error
            ? braced(tr("No picture"))
            : braced(background.errorText));
        ui->imageLabel->setOpacity(kOpaque);
    }
    else
    {
        QImage image = (background.canChangeAspectRatio() && background.cropToMonitorAspectRatio)
            ? background.croppedPreview
            : background.preview;

        ui->imageLabel->setPixmap(QPixmap::fromImage(image));
        ui->imageLabel->setOpacity(kOpacityPercent * background.opacityPercent);
    }
}

void LayoutBackgroundSettingsWidget::onImageDownloaded(const QString& filename, bool ok)
{
    const auto& state = m_store->state();

    // Check if loading was aborted or another image was selected.
    if (d->cache->getFullPath(filename) != state.background.imageSourcePath)
        return;

    if (ok)
    {
        m_store->imageDownloaded();
        loadPreview();
    }
    else
    {
        m_store->setBackgroundImageError(tr("Error while loading picture"));
    }
}

void LayoutBackgroundSettingsWidget::onImageUploaded(const QString& filename, bool ok)
{
    if (!ok)
    {
        m_store->setBackgroundImageError(tr("Error while uploading picture"));
        return;
    }

    appContext()->localSettings()->layoutKeepAspectRatio =
        ui->keepAspectRatioCheckBox->isChecked();
    LayoutSettingsDialogStateReducer::setKeepBackgroundAspectRatio(
        appContext()->localSettings()->layoutKeepAspectRatio());

    m_store->imageUploaded(filename);
    emit newImageUploadedSuccessfully();
}

void LayoutBackgroundSettingsWidget::loadPreview()
{
    const auto& state = m_store->state();

    auto loader = new ThreadedImageLoader(this);
    loader->setInput(state.background.imageSourcePath);
    loader->setTransformationMode(Qt::FastTransformation);
    loader->setSize(ui->imageLabel->contentSize());
    loader->setFlags(ThreadedImageLoader::TouchSizeFromOutside);
    connect(
        loader,
        &ThreadedImageLoader::imageLoaded,
        this,
        &LayoutBackgroundSettingsWidget::setPreview);
    connect(loader, &ThreadedImageLoader::imageLoaded, loader, &QObject::deleteLater);
    loader->start();
}

void LayoutBackgroundSettingsWidget::startDownloading()
{
    // Check if startDownloading() was queued several times.
    const auto& state = m_store->state();
    if (!state.background.canStartDownloading())
        return;

    d->cache->downloadFile(state.background.filename);
    m_store->startDownloading(d->cache->getFullPath(state.background.filename));
}

void LayoutBackgroundSettingsWidget::viewFile()
{
    const auto& state = m_store->state();
    NX_ASSERT(state.background.imagePresent());

    QString path = QLatin1String("file:///") + state.background.imageSourcePath;
    if (QDesktopServices::openUrl(QUrl(path)))
        return;

    auto dialog = createSelfDestructingDialog<QnImagePreviewDialog>(this);
    dialog->openImage(state.background.imageSourcePath);
    dialog->show();
}

void LayoutBackgroundSettingsWidget::selectFile()
{
    auto dialog = createSelfDestructingDialog<QnCustomFileDialog>(
        this,
        tr("Select file..."),
        appContext()->localSettings()->backgroundsFolder(),
        QnCustomFileDialog::createFilter(QnCustomFileDialog::picturesFilter()));
    dialog->setFileMode(QFileDialog::ExistingFile);

    connect(
        dialog,
        &QnCustomFileDialog::accepted,
        this,
        [this, dialog]
        {
            QString fileName = dialog->selectedFile();
            if (fileName.isEmpty())
                return;

            appContext()->localSettings()->backgroundsFolder = QFileInfo(fileName).absolutePath();

            QFileInfo fileInfo(fileName);
            if (fileInfo.size() == 0)
            {
                m_store->setBackgroundImageError(tr("Picture cannot be read"));
                return;
            }

            //// Check if we were disconnected (server shut down) while the dialog was open.
            //if (!d->layout->isFile() && !context()->user())
            //    return;
            m_store->imageSelected(fileName);
            loadPreview();
        });

    dialog->show();
}

void LayoutBackgroundSettingsWidget::setPreview(const QImage& image)
{
    const auto& state = m_store->state();
    switch (state.background.status)
    {
        case BackgroundImageStatus::loading:
        case BackgroundImageStatus::newImageLoading:
            break;
        default:
            // Status already changed.
            return;
    }

    if (image.isNull())
        m_store->setBackgroundImageError(tr("Picture cannot be loaded"));
    else
        m_store->setPreview(image);
}

} // namespace nx::vms::client::desktop
