#include "layout_settings_dialog.h"
#include "ui_layout_settings_dialog.h"

#include <QtCore/qmath.h>
#include <QtCore/QUrl>
#include <QtWidgets/QResizeEvent>

#include <QtGui/QDesktopServices>
#include <QtGui/QPainter>
#include <QtGui/QPen>
#include <QtGui/QPaintEvent>
#include <QtGui/QImageReader>

#include <client/client_settings.h>
#include <core/resource/layout_resource.h>

#include <ui/dialogs/image_preview_dialog.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/style/globals.h>
#include <ui/widgets/framed_label.h>

#include <utils/threaded_image_loader.h>
#include <utils/app_server_image_cache.h>
#include <utils/local_file_cache.h>
#include <utils/image_transformations.h>
#include <utils/common/scoped_value_rollback.h>

namespace {
    /** Possible states of the dialog contents. */
    enum DialogState {
        /** No image is selected. */
        NoImage,
        /** Error is occured. */
        Error,
        /** Current layout image is being downloaded from the EC. */
        ImageDownloading,
        /** Current layout image is downloaded from the EC. */
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
        /** New image is selected being uploading to the EC. */
        NewImageUploading
    };
}



class QnLayoutSettingsDialogPrivate
{
public:
    QnLayoutSettingsDialogPrivate():
        state(NoImage),
        cellAspectRatio(qnGlobals->defaultLayoutCellAspectRatio())
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
};


QnLayoutSettingsDialog::QnLayoutSettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QnLayoutSettingsDialog),
    d_ptr(new QnLayoutSettingsDialogPrivate()),
    m_cache(NULL),
    m_isUpdating(false)
{
    ui->setupUi(this);

    ui->imageLabel->installEventFilter(this);


    ui->widthSpinBox->setMinimum(qnGlobals->layoutBackgroundMinSize().width());
    ui->widthSpinBox->setMaximum(qnGlobals->layoutBackgroundMaxSize().width());

    ui->heightSpinBox->setMinimum(qnGlobals->layoutBackgroundMinSize().height());
    ui->heightSpinBox->setMaximum(qnGlobals->layoutBackgroundMaxSize().height());

    connect(ui->viewButton,                 SIGNAL(clicked()),          this, SLOT(viewFile()));
    connect(ui->selectButton,               SIGNAL(clicked()),          this, SLOT(selectFile()));
    connect(ui->clearButton,                SIGNAL(clicked()),          this, SLOT(at_clearButton_clicked()));
    connect(ui->lockedCheckBox,             SIGNAL(clicked()),          this, SLOT(updateControls()));
    connect(ui->buttonBox,                  SIGNAL(accepted()),         this, SLOT(at_accepted()));
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
    Q_D(const QnLayoutSettingsDialog);
    if (target == ui->imageLabel &&
            event->type() == QEvent::MouseButtonRelease) {
        if (!ui->lockedCheckBox->isChecked() && (d->state == NoImage || d->state == Error) )
            selectFile();
        else
            viewFile();
    }
    return base_type::eventFilter(target, event);
}

void QnLayoutSettingsDialog::readFromResource(const QnLayoutResourcePtr &layout) {
    Q_D(QnLayoutSettingsDialog);

    m_cache = layout->hasFlags(QnResource::url | QnResource::local | QnResource::layout) //TODO: #GDM refactor duplicated code
            ? new QnLocalFileCache(this)
            : new QnAppServerImageCache(this);
    connect(m_cache, SIGNAL(fileDownloaded(QString, bool)), this, SLOT(at_imageLoaded(QString, bool)));
    connect(m_cache, SIGNAL(fileUploaded(QString, bool)), this, SLOT(at_imageStored(QString, bool)));

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

    if (layout->cellAspectRatio() > 0) {
        qreal cellWidth = 1.0 + layout->cellSpacing().width();
        qreal cellHeight = 1.0 / layout->cellAspectRatio() + layout->cellSpacing().height();
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
    // TODO: #GDM remove unused image if any
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
    QnScopedValueRollback<bool> guard(&m_isUpdating, true);
    Q_UNUSED(guard)

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
    ui->cropToMonitorCheckBox->setEnabled(imagePresent && d->canChangeAspectRatio());
    // image is already cropped to monitor aspect ratio
    if (imagePresent && !d->canChangeAspectRatio())
        ui->cropToMonitorCheckBox->setChecked(true);

    QImage image;
    if (!imagePresent) {
        ui->imageLabel->setPixmap(QPixmap());
        ui->imageLabel->setText(d->state != Error
                            ? tr("<No image>")
                            : d->errorText);
    } else {
        image = (d->canChangeAspectRatio() && ui->cropToMonitorCheckBox->isChecked())
                ? d->croppedPreview
                : d->preview;
        ui->imageLabel->setPixmap(QPixmap::fromImage(image.scaled(ui->imageLabel->size(), Qt::KeepAspectRatio, Qt::FastTransformation)));
    }

    qreal targetAspectRatio = bestAspectRatioForCells();
    // TODO: #GDM do not change if values were changed manually?
    if (ui->keepAspectRatioCheckBox->isChecked() && targetAspectRatio > 0 && !cellsAreBestAspected()) {
        int w, h;
        if (targetAspectRatio >= 1.0) { // width is greater than height
            w = qnGlobals->layoutBackgroundMaxSize().width();
            h = qRound((qreal)w / targetAspectRatio);
        } else {
            h = qnGlobals->layoutBackgroundMaxSize().height();
            w = qRound((qreal)h * targetAspectRatio);
        }
        ui->widthSpinBox->setMaximum(w);
        ui->heightSpinBox->setMaximum(h);

        // limit w*h <= recommended area; minor variations are allowed, e.g. 17*6 ~~= 100;
        qreal areaCoef = qSqrt((qreal)w * h / qnGlobals->layoutBackgroundRecommendedArea());
        if (areaCoef > 1.0) {
            w = qRound((qreal)w / areaCoef);
            h = qRound((qreal)h / areaCoef);
        }
        ui->widthSpinBox->setValue(w);
        ui->heightSpinBox->setValue(h);
    } else {
        ui->widthSpinBox->setMaximum(qnGlobals->layoutBackgroundMaxSize().width());
        ui->heightSpinBox->setMaximum(qnGlobals->layoutBackgroundMaxSize().height());
    }

}

void QnLayoutSettingsDialog::at_clearButton_clicked() {
    Q_D(QnLayoutSettingsDialog);
    d->clear();
    updateControls();
}

void QnLayoutSettingsDialog::at_accepted() {
    Q_D(QnLayoutSettingsDialog);

    switch (d->state) {

    /* if image not present or still not loaded then do nothing */
    case NoImage:
    case Error:
    case NewImageSelected:
    case ImageDownloaded:
        accept();
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
        accept();
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
    ui->imageLabel->setOpacityPercent(value);
}

void QnLayoutSettingsDialog::at_widthSpinBox_valueChanged(int value) {
    if (!ui->keepAspectRatioCheckBox->isChecked())
        return;
    if (m_isUpdating)
        return;
    QnScopedValueRollback<bool> guard(&m_isUpdating, true);
    Q_UNUSED(guard)

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
    QnScopedValueRollback<bool> guard(&m_isUpdating, true);
    Q_UNUSED(guard)

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
        d->errorText = tr("<Image cannot be loaded>");
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
        d->errorText = tr("<Image cannot be uploaded>");
        updateControls();
        return;
    }
    d->imageFilename = filename;
    d->state = NewImageLoaded;
    qnSettings->setLayoutKeepAspectRatio(ui->keepAspectRatioCheckBox->isChecked());
    //updateControls() is not needed, we are closing the dialog here
    accept();
}

void QnLayoutSettingsDialog::loadPreview() {
    if (!this->isVisible())
        return;
    Q_D(QnLayoutSettingsDialog);
    if (!d->imageFileIsAvailable() || d->imageIsLoading())
        return;

    QnThreadedImageLoader* loader = new QnThreadedImageLoader(this);
    loader->setInput(d->imageSourcePath);
    loader->setTransformationMode(Qt::FastTransformation);
    loader->setSize(ui->imageLabel->size());
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

    QnImagePreviewDialog dialog;
    dialog.openImage(d->imageSourcePath);
    dialog.exec();
}

void QnLayoutSettingsDialog::selectFile() {
    Q_D(QnLayoutSettingsDialog);

    QScopedPointer<QnCustomFileDialog> dialog(new QnCustomFileDialog(this, tr("Open file")));
    dialog->setFileMode(QFileDialog::ExistingFile);

    QString nameFilter;
    foreach (const QByteArray &format, QImageReader::supportedImageFormats()) {
        if (!nameFilter.isEmpty())
            nameFilter += QLatin1Char(' ');
        nameFilter += QLatin1String("*.") + QLatin1String(format);
    }
    nameFilter = QLatin1Char('(') + nameFilter + QLatin1Char(')');
    dialog->setNameFilter(tr("Pictures %1").arg(nameFilter));

    bool cropImage = ui->cropToMonitorCheckBox->isChecked();
    dialog->addCheckBox(tr("Crop to current monitor AR"), &cropImage);
    if(!dialog->exec())
        return;

    QStringList files = dialog->selectedFiles();
    if (files.size() < 0)
        return;

    d->clear();
    d->imageSourcePath = files[0];
    d->imageFilename = QString();
    d->state = NewImageSelected;
    ui->cropToMonitorCheckBox->setChecked(cropImage);

    loadPreview();
    updateControls();
}

void QnLayoutSettingsDialog::setPreview(const QImage &image) {
    Q_D(QnLayoutSettingsDialog);
    if (d->state != ImageLoading && d->state != NewImageLoading)
        return;

    if (image.isNull()) {
        d->state = Error;
        d->errorText = tr("<Image cannot be loaded>");
        updateControls();
        return;
    }

    if (d->state == ImageLoading)
        d->state = ImageLoaded;
    else if (d->state = NewImageLoading)
        d->state = NewImageLoaded;
    d->preview = image;

    /* Disable cropping for images that are quite well aspected. */
    qreal imageAspectRatio = (qreal)image.width() / (qreal)image.height();
    if (qAbs(imageAspectRatio - screenAspectRatio()) > 0.05)
        d->croppedPreview = cropImageToAspectRatio(image, screenAspectRatio());

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

