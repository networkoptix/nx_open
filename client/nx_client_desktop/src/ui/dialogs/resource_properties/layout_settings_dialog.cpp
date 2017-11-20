#include "layout_settings_dialog.h"
#include "ui_layout_settings_dialog.h"

#include <QtCore/QtMath>
#include <QtCore/QUrl>
#include <QtGui/QResizeEvent>

#include <QtGui/QDesktopServices>
#include <QtGui/QPainter>
#include <QtGui/QPen>
#include <QtGui/QPaintEvent>
#include <QtGui/QImageReader>

#include <QtWidgets/QDesktopWidget>

#include <client/client_settings.h>
#include <core/resource/layout_resource.h>

#include <ui/common/image_processing.h>
#include <ui/dialogs/image_preview_dialog.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/file_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/globals.h>
#include <ui/widgets/common/framed_label.h>
#include <ui/workbench/workbench_context.h>

#include <utils/threaded_image_loader.h>
#include <nx/client/core/utils/geometry.h>
#include <nx/client/desktop/utils/server_image_cache.h>
#include <nx/client/desktop/utils/local_file_cache.h>
#include <utils/common/scoped_value_rollback.h>

using namespace nx::client::desktop;

namespace {

QString braced(const QString& source)
{
    return L'<' + source + L'>';
};

    /** Possible states of the dialog contents. */
    enum DialogState {
        /** No image is selected. */
        NoImage,
        /** Error is occurred. */
        Error,
        /** Current layout image is being downloaded from the Server. */
        ImageDownloading,
        /** Current layout image is downloaded from the Server. */
        ImageDownloaded,
        /** Current layout image is downloaded and being loading. */
        ImageLoading,
        /** Current layout image is loaded. */
        ImageLoaded,
        /** New image is selected. */
        NewImageSelected,
        /** New image is selected and being loading. */
        NewImageLoading,
        /** New image is selected and loaded. */
        NewImageLoaded,
        /** New image is selected being uploading to the Server. */
        NewImageUploading
    };

    /** If difference between image AR and screen AR is smaller than this value, cropped preview will not be generated */
    const qreal aspectRatioVariation = 0.05;

}



class QnLayoutSettingsDialogPrivate
{
public:
    QnLayoutSettingsDialogPrivate():
        state(NoImage),
        cellAspectRatio(qnGlobals->defaultLayoutCellAspectRatio()),
        skipNextReleaseEvent(false)
    {}

    virtual ~QnLayoutSettingsDialogPrivate(){}

    void clear() {
        state = NoImage;
        imageFilename = QString();
        imageSourcePath = QString();
        preview = QImage();
        croppedPreview = QImage();
        errorText = QString();
    }

    /** Image is present and image file is available locally. */
    bool imageFileIsAvailable() const {
        return state != NoImage && state != Error && state != ImageDownloading;
    }

    bool imageIsLoading() const {
        return
            state == ImageDownloading ||
            state == ImageLoading ||
            state == NewImageLoading ||
            state == NewImageUploading;
    }

    bool canChangeAspectRatio() const {
        return !croppedPreview.isNull();
    }

    /** Whether we can crop the image.
     *  The only case when we cannot - if we open already cropped and saved image.
     */
    bool canCropImage() const {
        bool loadedImageIsCropped = (state == ImageLoaded && croppedPreview.isNull());
        return !loadedImageIsCropped;
    }

    DialogState state;

    /** Path to the selected image (may be path in cache). */
    QString imageSourcePath;

    /** Filename of the image (GUID-generated). */
    QString imageFilename;

    /** Cell aspect ratio for the current layout. */
    qreal cellAspectRatio;

    /** Preview image */
    QImage preview;

    /** Cropped preview image. */
    QImage croppedPreview;

    /** Text of the last error if any occured. */
    QString errorText;

    bool skipNextReleaseEvent;
};


QnLayoutSettingsDialog::QnLayoutSettingsDialog(QWidget *parent) :
    base_type(parent),
    ui(new Ui::LayoutSettingsDialog),
    d_ptr(new QnLayoutSettingsDialogPrivate()),
    m_cache(NULL),
    m_isUpdating(false)
{
    ui->setupUi(this);

    ui->widthSpinBox->setSuffix(L' ' + tr("cells"));
    ui->heightSpinBox->setSuffix(L' ' + tr("cells"));

    setHelpTopic(ui->lockedCheckBox,        Qn::LayoutSettings_Locking_Help);
    setHelpTopic(ui->backgroundGroupBox,    Qn::LayoutSettings_EMapping_Help);

    installEventFilter(this);
    ui->imageLabel->installEventFilter(this);
    ui->imageLabel->setFrameColor(qnGlobals->frameColor());

    ui->widthSpinBox->setMinimum(qnGlobals->layoutBackgroundMinSize().width());
    ui->widthSpinBox->setMaximum(qnGlobals->layoutBackgroundMaxSize().width());

    ui->heightSpinBox->setMinimum(qnGlobals->layoutBackgroundMinSize().height());
    ui->heightSpinBox->setMaximum(qnGlobals->layoutBackgroundMaxSize().height());

    connect(ui->viewButton,                 SIGNAL(clicked()),          this, SLOT(viewFile()));
    connect(ui->selectButton,               SIGNAL(clicked()),          this, SLOT(selectFile()));
    connect(ui->clearButton,                SIGNAL(clicked()),          this, SLOT(at_clearButton_clicked()));
    connect(ui->lockedCheckBox,             SIGNAL(clicked()),          this, SLOT(updateControls()));
    connect(ui->opacitySpinBox,             SIGNAL(valueChanged(int)),  this, SLOT(at_opacitySpinBox_valueChanged(int)));
    connect(ui->widthSpinBox,               SIGNAL(valueChanged(int)),  this, SLOT(at_widthSpinBox_valueChanged(int)));
    connect(ui->heightSpinBox,              SIGNAL(valueChanged(int)),  this, SLOT(at_heightSpinBox_valueChanged(int)));
    connect(ui->cropToMonitorCheckBox,      SIGNAL(toggled(bool)),      this, SLOT(updateControls()));
    connect(ui->keepAspectRatioCheckBox,    SIGNAL(toggled(bool)),      this, SLOT(updateControls()));

    updateControls();
}

QnLayoutSettingsDialog::~QnLayoutSettingsDialog()
{
}

bool QnLayoutSettingsDialog::eventFilter(QObject *target, QEvent *event) {
    Q_D(QnLayoutSettingsDialog);

    if (event->type() == QEvent::LeaveWhatsThisMode)
        d->skipNextReleaseEvent = true;
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        if (target == ui->imageLabel && !d->skipNextReleaseEvent)
        {
            if (!ui->lockedCheckBox->isChecked() && (d->state == NoImage || d->state == Error) )
                selectFile();
            else
                viewFile();
        }
        d->skipNextReleaseEvent = false;
    }

    return base_type::eventFilter(target, event);
}

void QnLayoutSettingsDialog::readFromResource(const QnLayoutResourcePtr &layout) {
    Q_D(QnLayoutSettingsDialog);

    m_cache = layout->isFile()
            ? new LocalFileCache(this)
            : new ServerImageCache(this);

    connect(m_cache, &ServerFileCache::fileDownloaded, this,
        [this](const QString &filename, ServerFileCache::OperationResult status)
        {
            at_imageLoaded(filename, status == ServerFileCache::OperationResult::ok);
        });

    connect(m_cache, &ServerFileCache::fileUploaded, this,
        [this](const QString &filename, ServerFileCache::OperationResult status)
        {
            at_imageStored(filename, status == ServerFileCache::OperationResult::ok);
        });

    d->clear();
    d->imageFilename = layout->backgroundImageFilename();

    if (!d->imageFilename.isEmpty()) {
        d->imageSourcePath = m_cache->getFullPath(d->imageFilename);
        d->state = ImageDownloading;
        m_cache->downloadFile(d->imageFilename);

        ui->widthSpinBox->setMaximum(qnGlobals->layoutBackgroundMaxSize().width());
        ui->heightSpinBox->setMaximum(qnGlobals->layoutBackgroundMaxSize().height());
        ui->widthSpinBox->setValue(layout->backgroundSize().width());
        ui->heightSpinBox->setValue(layout->backgroundSize().height());
        ui->opacitySpinBox->setValue(layout->backgroundOpacity() * 100);
        ui->keepAspectRatioCheckBox->setChecked(false);
    }
    ui->lockedCheckBox->setChecked(layout->locked());

    if (layout->hasCellAspectRatio())
    {
        const auto spacing = layout->cellSpacing();
        qreal cellWidth = 1.0 + spacing;
        qreal cellHeight = 1.0 / layout->cellAspectRatio() + spacing;
        d->cellAspectRatio = cellWidth / cellHeight;
    }

    updateControls();
}

bool QnLayoutSettingsDialog::submitToResource(const QnLayoutResourcePtr &layout) {
    Q_D(const QnLayoutSettingsDialog);

    if (!hasChanges(layout))
        return false;

    layout->setLocked(ui->lockedCheckBox->isChecked());
    layout->setBackgroundImageFilename(d->imageFilename);
    layout->setBackgroundSize(QSize(ui->widthSpinBox->value(), ui->heightSpinBox->value()));
    layout->setBackgroundOpacity((qreal)ui->opacitySpinBox->value() * 0.01);
    // TODO: #GDM #Common remove unused image if any
    return true;
}

qreal QnLayoutSettingsDialog::screenAspectRatio() const {
    QRect screen = qApp->desktop()->screenGeometry();
    return (qreal)screen.width() / (qreal)screen.height();
}

qreal QnLayoutSettingsDialog::bestAspectRatioForCells() const {
    Q_D(const QnLayoutSettingsDialog);
    if (d->state != ImageLoaded && d->state != NewImageLoaded)
        return -1;
    QImage image = (d->canChangeAspectRatio() && ui->cropToMonitorCheckBox->isChecked())
            ? d->croppedPreview
            : d->preview;
    if (image.isNull())
        return -2;
    qreal aspectRatio = (qreal)image.width() / (qreal)image.height();
    return aspectRatio / d->cellAspectRatio;
}

bool QnLayoutSettingsDialog::cellsAreBestAspected() const {
    qreal targetAspectRatio = bestAspectRatioForCells();
    if (targetAspectRatio < 0)
        return false;
    int w = qRound((qreal)ui->heightSpinBox->value() * targetAspectRatio);
    int h = qRound((qreal)ui->widthSpinBox->value() / targetAspectRatio);
    return (w == ui->widthSpinBox->value() || h == ui->heightSpinBox->value());
}

bool QnLayoutSettingsDialog::hasChanges(const QnLayoutResourcePtr &layout) {
    Q_D(const QnLayoutSettingsDialog);
    if (
            (ui->lockedCheckBox->isChecked() != layout->locked()) ||
            (ui->opacitySpinBox->value() != int(layout->backgroundOpacity() * 100)) ||
            (d->imageFilename != layout->backgroundImageFilename())
            )
        return true;

    // do not save size change if no image was set
    QSize newSize(ui->widthSpinBox->value(), ui->heightSpinBox->value());
    return (d->state != NoImage && newSize != layout->backgroundSize());
}

void QnLayoutSettingsDialog::updateControls() {
    if (m_isUpdating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_isUpdating, true);

    Q_D(const QnLayoutSettingsDialog);

    setProgress(d->imageIsLoading());
    ui->generalGroupBox->setEnabled(d->state != NewImageUploading);
    ui->buttonBox->setEnabled(d->state != NewImageUploading);

    if (d->imageIsLoading())
        return;

    bool imagePresent = d->imageFileIsAvailable();
    bool locked = ui->lockedCheckBox->isChecked();

    ui->widthSpinBox->setEnabled(imagePresent && !locked);
    ui->heightSpinBox->setEnabled(imagePresent && !locked);
    ui->keepAspectRatioCheckBox->setEnabled(imagePresent && !locked);
    ui->viewButton->setEnabled(imagePresent);
    ui->clearButton->setEnabled(imagePresent && !locked);
    ui->opacitySpinBox->setEnabled(imagePresent && !locked);
    ui->selectButton->setEnabled(!locked);

    if (!d->canCropImage()) /* Mark image as already cropped. */
        ui->cropToMonitorCheckBox->setChecked(true);
    ui->cropToMonitorCheckBox->setEnabled(imagePresent && !locked && d->canCropImage());

    QImage image;
    if (!imagePresent) {
        ui->imageLabel->setPixmap(QPixmap());
        ui->imageLabel->setText(d->state != Error
                            ? braced(tr("No picture"))
                            : d->errorText);
    } else {
        image = (d->canChangeAspectRatio() && ui->cropToMonitorCheckBox->isChecked())
                ? d->croppedPreview
                : d->preview;
        ui->imageLabel->setPixmap(QPixmap::fromImage(image.scaled(ui->imageLabel->contentSize(), Qt::KeepAspectRatio, Qt::FastTransformation)));
    }

    qreal targetAspectRatio = bestAspectRatioForCells();
    // TODO: #GDM #Common do not change if values were changed manually?
    if (ui->keepAspectRatioCheckBox->isChecked() && targetAspectRatio > 0 && !cellsAreBestAspected()) {
        QSize minSize = nx::client::core::Geometry::expanded(
            targetAspectRatio,
            qnGlobals->layoutBackgroundMinSize(),
            Qt::KeepAspectRatioByExpanding).toSize();
        QSize maxSize = nx::client::core::Geometry::expanded(
            targetAspectRatio,
            qnGlobals->layoutBackgroundMaxSize(),
            Qt::KeepAspectRatio).toSize();

        ui->widthSpinBox->setRange(minSize.width(), maxSize.width());
        ui->heightSpinBox->setRange(minSize.height(), maxSize.height());

        // limit w*h <= recommended area; minor variations are allowed, e.g. 17*6 ~~= 100;
        qreal height = qSqrt(qnGlobals->layoutBackgroundRecommendedArea() / targetAspectRatio);
        qreal width = height * targetAspectRatio;
        ui->widthSpinBox->setValue(width);
        ui->heightSpinBox->setValue(height);
    }
    else
    {
        ui->widthSpinBox->setMaximum(qnGlobals->layoutBackgroundMaxSize().width());
        ui->heightSpinBox->setMaximum(qnGlobals->layoutBackgroundMaxSize().height());

        if (!ui->keepAspectRatioCheckBox->isChecked())
        {
            ui->widthSpinBox->setMinimum(qnGlobals->layoutBackgroundMinSize().width());
            ui->heightSpinBox->setMinimum(qnGlobals->layoutBackgroundMinSize().height());
        }
    }

}

void QnLayoutSettingsDialog::at_clearButton_clicked() {
    Q_D(QnLayoutSettingsDialog);
    d->clear();
    updateControls();
}


void QnLayoutSettingsDialog::accept() {
    Q_D(QnLayoutSettingsDialog);

    switch (d->state) {

    /* if image not present or still not loaded then do nothing */
    case NoImage:
    case Error:
    case NewImageSelected:
    case ImageDownloaded:
        base_type::accept();
        return;

    /* current progress should be cancelled before accepting */
    case ImageDownloading:
    case ImageLoading:
    case NewImageLoading:
    case NewImageUploading:
        return;

    case ImageLoaded:
        /* Current image should be cropped and re-uploaded. */
        if (d->canChangeAspectRatio() && ui->cropToMonitorCheckBox->isChecked())
            break;
        qnSettings->setLayoutKeepAspectRatio(ui->keepAspectRatioCheckBox->isChecked());
        base_type::accept();
        return;
    case NewImageLoaded:
        break;
    }

    if (ui->cropToMonitorCheckBox->isChecked())
        m_cache->storeImage(d->imageSourcePath, screenAspectRatio());
    else
        m_cache->storeImage(d->imageSourcePath);
    d->state = NewImageUploading;
    updateControls();
}

void QnLayoutSettingsDialog::at_opacitySpinBox_valueChanged(int value) {
    ui->imageLabel->setOpacity(0.01 * value);
}

void QnLayoutSettingsDialog::at_widthSpinBox_valueChanged(int value) {
    if (!ui->keepAspectRatioCheckBox->isChecked())
        return;
    if (m_isUpdating)
        return;
    QN_SCOPED_VALUE_ROLLBACK(&m_isUpdating, true);

    qreal targetAspectRatio = bestAspectRatioForCells();
    if (targetAspectRatio < 0)
        return;
    int h = qRound((qreal)value / targetAspectRatio);
    ui->heightSpinBox->setValue(h);
}

void QnLayoutSettingsDialog::at_heightSpinBox_valueChanged(int value) {
    if (!ui->keepAspectRatioCheckBox->isChecked())
        return;
    if (m_isUpdating)
        return;
    QN_SCOPED_VALUE_ROLLBACK(&m_isUpdating, true);

    qreal targetAspectRatio = bestAspectRatioForCells();
    if (targetAspectRatio < 0)
        return;
    int w = qRound((qreal)value * targetAspectRatio);
    ui->widthSpinBox->setValue(w);
}


void QnLayoutSettingsDialog::at_imageLoaded(const QString &filename, bool ok) {
    Q_D(QnLayoutSettingsDialog);

    if (m_cache->getFullPath(filename) != d->imageSourcePath)
        return;
    d->state = ImageDownloaded;

    if (!ok) {
        d->state = Error;
        d->errorText = braced(tr("Error while loading picture"));
    } else {
        d->state = ImageDownloaded;
    }
    loadPreview();
    updateControls();
}

void QnLayoutSettingsDialog::at_imageStored(const QString &filename, bool ok) {
    Q_D(QnLayoutSettingsDialog);

    if (!ok) {
        d->state = Error;
        d->errorText = braced(tr("Error while uploading picture"));
        updateControls();
        return;
    }
    d->imageFilename = filename;
    d->state = NewImageLoaded;
    qnSettings->setLayoutKeepAspectRatio(ui->keepAspectRatioCheckBox->isChecked());
    //updateControls() is not needed, we are closing the dialog here
    base_type::accept();
}

void QnLayoutSettingsDialog::loadPreview() {
    Q_D(QnLayoutSettingsDialog);
    if (!d->imageFileIsAvailable() || d->imageIsLoading())
        return;

    QnThreadedImageLoader* loader = new QnThreadedImageLoader(this);
    loader->setInput(d->imageSourcePath);
    loader->setTransformationMode(Qt::FastTransformation);
    loader->setSize(ui->imageLabel->contentSize());
    loader->setFlags(Qn::TouchSizeFromOutside);
    connect(loader, SIGNAL(finished(QImage)), this, SLOT(setPreview(QImage)));
    loader->start();

    if (d->state == ImageDownloaded)
        d->state = ImageLoading;
    else if (d->state == NewImageSelected)
        d->state = NewImageLoading;
}

void QnLayoutSettingsDialog::viewFile() {
    Q_D(const QnLayoutSettingsDialog);
    if (!d->imageFileIsAvailable())
        return;

    QString path = QLatin1String("file:///") + d->imageSourcePath;
    if (QDesktopServices::openUrl(QUrl(path)))
        return;

    QnImagePreviewDialog dialog(this);
    dialog.openImage(d->imageSourcePath);
    dialog.exec();
}

void QnLayoutSettingsDialog::selectFile() {
    Q_D(QnLayoutSettingsDialog);

    QString nameFilter;
    foreach (const QByteArray &format, QImageReader::supportedImageFormats()) {
        if (!nameFilter.isEmpty())
            nameFilter += QLatin1Char(' ');
        nameFilter += QLatin1String("*.") + QLatin1String(format);
    }
    nameFilter = QLatin1Char('(') + nameFilter + QLatin1Char(')');

    QScopedPointer<QnSessionAwareFileDialog> dialog(
        new QnSessionAwareFileDialog (
            this, tr("Select file..."),
            qnSettings->backgroundsFolder(),
            tr("Pictures %1").arg(nameFilter)
            )
        );
    dialog->setFileMode(QFileDialog::ExistingFile);

    if(!dialog->exec())
        return;

    QString fileName = dialog->selectedFile();
    if (fileName.isEmpty())
        return;

    qnSettings->setBackgroundsFolder(QFileInfo(fileName).absolutePath());

    QFileInfo fileInfo(fileName);
    if (fileInfo.size() == 0) {
        d->state = Error;
        d->errorText = braced(tr("Picture cannot be read"));
        updateControls();
        return;
    }

    if (fileInfo.size() > ServerFileCache::maximumFileSize()) {
        d->state = Error;
        // TODO: #GDM #3.1 move out strings and logic to separate class (string.h:bytesToString)
        //Important: maximumFileSize() is hardcoded in 1024-base
        d->errorText = braced(tr("Picture is too big. Maximum size is %1 MB")
            .arg(ServerFileCache::maximumFileSize() / (1024*1024)));
        updateControls();
        return;
    }

    /* Check if we were disconnected (server shut down) while the dialog was open. */
    if (!context()->user())
        return;

    d->clear();
    d->imageSourcePath = fileName;
    d->imageFilename = QString();
    d->state = NewImageSelected;

    loadPreview();
    updateControls();
}

void QnLayoutSettingsDialog::setPreview(const QImage &image) {
    Q_D(QnLayoutSettingsDialog);
    if (d->state != ImageLoading && d->state != NewImageLoading)
        return;

    if (image.isNull()) {
        d->state = Error;
        d->errorText = braced(tr("Picture cannot be loaded"));
        updateControls();
        return;
    }

    if (d->state == ImageLoading)
        d->state = ImageLoaded;
    else if (d->state == NewImageLoading)
        d->state = NewImageLoaded;
    d->preview = image;

    /* Disable cropping for images that are quite well aspected. */
    qreal imageAspectRatio = (qreal)image.width() / (qreal)image.height();
    if (qAbs(imageAspectRatio - screenAspectRatio()) > aspectRatioVariation) {
        d->croppedPreview = cropToAspectRatio(image, screenAspectRatio());
    }
    else {
        d->croppedPreview = QImage();
    }

    if (d->state == NewImageLoaded) {
        // always set flag to true for new images
        ui->keepAspectRatioCheckBox->setChecked(true);
    } else if (!qnSettings->layoutKeepAspectRatio()) {
        // for old - set to false if previous accepted value was false
        ui->keepAspectRatioCheckBox->setChecked(false);
    } else {
        // set to true if possible (current sizes will not be changed)
        ui->keepAspectRatioCheckBox->setChecked(cellsAreBestAspected());
    }

    updateControls();
}

void QnLayoutSettingsDialog::setProgress(bool value) {
    ui->stackedWidget->setCurrentIndex(value ? 1 : 0);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!value);
}
