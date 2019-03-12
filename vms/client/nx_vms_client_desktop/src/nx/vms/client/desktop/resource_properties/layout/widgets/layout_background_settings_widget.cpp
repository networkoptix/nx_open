#include "layout_background_settings_widget.h"
#include "ui_layout_background_settings_widget.h"

#include <QtCore/QtMath>
#include <QtCore/QUrl>

#include <QtGui/QDesktopServices>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>

#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

#include <client/client_settings.h>

#include <ui/dialogs/image_preview_dialog.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/globals.h>
#include <ui/widgets/common/framed_label.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workbench/workbench_context.h>

#include <nx/vms/client/desktop/image_providers/threaded_image_loader.h>
#include <nx/vms/client/desktop/utils/server_image_cache.h>
#include <nx/vms/client/desktop/utils/local_file_cache.h>

#include <utils/common/delayed.h>

#include "../redux/layout_settings_dialog_store.h"
#include "../redux/layout_settings_dialog_state.h"
#include "../redux/layout_settings_dialog_state_reducer.h"

namespace nx::vms::client::desktop {

using State = LayoutSettingsDialogState;
using BackgroundImageStatus = State::BackgroundImageStatus;

namespace {

QString braced(const QString& source)
{
    return L'<' + source + L'>';
};

static constexpr qreal kOpaque = 1.0;
static constexpr qreal kOpacityPercent = 0.01;

//TODO: #GDM Code duplication.
// Aspect ratio of the current screen.
QnAspectRatio screenAspectRatio()
{
    const auto screen = qApp->desktop()->screenGeometry();
    return QnAspectRatio(screen.size());
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
        qnSettings->layoutKeepAspectRatio());

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

    ui->widthSpinBox->setSuffix(L' ' + tr("cells"));
    ui->heightSpinBox->setSuffix(L' ' + tr("cells"));

    setHelpTopic(this, Qn::LayoutSettings_EMapping_Help);

    installEventFilter(this);
    ui->imageLabel->installEventFilter(this);
    ui->imageLabel->setFrameColor(qnGlobals->frameColor());
    ui->imageLabel->setAutoScale(true);
}

void LayoutBackgroundSettingsWidget::initCache(bool isLocalFile)
{
    d->cache = isLocalFile
        ? new LocalFileCache(this)
        : new ServerImageCache(this);

    connect(
        d->cache,
        &ServerFileCache::fileDownloaded,
        this,
        [this](const QString& filename, ServerFileCache::OperationResult status)
        {
            at_imageLoaded(filename, status == ServerFileCache::OperationResult::ok);
        });

    connect(
        d->cache,
        &ServerFileCache::fileUploaded,
        this,
        [this](const QString& filename, ServerFileCache::OperationResult status)
        {
            at_imageStored(filename, status == ServerFileCache::OperationResult::ok);
        });
}

void LayoutBackgroundSettingsWidget::loadState(const LayoutSettingsDialogState& state)
{
    // Currently this dialog is not reusable.
    if (!d->cache)
        initCache(state.isLocalFile);

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

void LayoutBackgroundSettingsWidget::at_imageLoaded(const QString& filename, bool ok)
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

void LayoutBackgroundSettingsWidget::at_imageStored(const QString& filename, bool ok)
{
    if (!ok)
    {
        m_store->setBackgroundImageError(tr("Error while uploading picture"));
        return;
    }

    qnSettings->setLayoutKeepAspectRatio(ui->keepAspectRatioCheckBox->isChecked());
    LayoutSettingsDialogStateReducer::setKeepBackgroundAspectRatio(
        qnSettings->layoutKeepAspectRatio());

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

    QnImagePreviewDialog dialog(this);
    dialog.openImage(state.background.imageSourcePath);
    dialog.exec();
}

void LayoutBackgroundSettingsWidget::selectFile()
{
    QScopedPointer<QnCustomFileDialog> dialog(
        new QnCustomFileDialog(
            this,
            tr("Select file..."),
            qnSettings->backgroundsFolder(),
            QnCustomFileDialog::createFilter(QnCustomFileDialog::picturesFilter())));
    dialog->setFileMode(QFileDialog::ExistingFile);

    if (!dialog->exec())
        return;

    QString fileName = dialog->selectedFile();
    if (fileName.isEmpty())
        return;

    qnSettings->setBackgroundsFolder(QFileInfo(fileName).absolutePath());

    QFileInfo fileInfo(fileName);
    if (fileInfo.size() == 0)
    {
        m_store->setBackgroundImageError(tr("Picture cannot be read"));
        return;
    }

    if (fileInfo.size() > ServerFileCache::maximumFileSize())
    {
        m_store->setBackgroundImageError(
            tr("Picture is too big. Maximum size is %1 MB")
            .arg(ServerFileCache::maximumFileSize() / (1024 * 1024)));
        return;
    }

    //// Check if we were disconnected (server shut down) while the dialog was open.
    //if (!d->layout->isFile() && !context()->user())
    //    return;
    m_store->imageSelected(fileName);
    loadPreview();
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
