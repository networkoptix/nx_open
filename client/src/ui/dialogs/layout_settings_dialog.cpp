#include "layout_settings_dialog.h"
#include "ui_layout_settings_dialog.h"

#include <QtCore/qmath.h>
#include <QUrl>

#include <QtGui/QDesktopServices>
#include <QtGui/QPainter>
#include <QtGui/QPen>
#include <QtGui/QPaintEvent>
#include <QImageReader>

#include <core/resource/layout_resource.h>

#include <ui/dialogs/image_preview_dialog.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/style/globals.h>

#include <utils/threaded_image_loader.h>
#include <utils/app_server_image_cache.h>
#include <utils/local_file_cache.h>
#include <utils/image_transformations.h>

namespace {
    const int labelFrameWidth = 4;  // in pixels
}

class QnFramedLabel: public QLabel {
    typedef QLabel base_type;
public:
    explicit QnFramedLabel(QWidget* parent = 0):
        base_type(parent),
        m_opacityPercent(100)
    {}
    ~QnFramedLabel() {}

    QSize size() const {
        return base_type::size() - QSize(labelFrameWidth*2, labelFrameWidth*2);
    }

    int opacityPercent() const {
        return m_opacityPercent;
    }

    void setOpacityPercent(int value) {
        if (m_opacityPercent == value)
            return;
        m_opacityPercent = value;
        repaint();
    }

protected:
    virtual void paintEvent(QPaintEvent *event) override {
        bool pixmapExists = pixmap() && !pixmap()->isNull();
        if (!pixmapExists)
            base_type::paintEvent(event);

        QPainter painter(this);
        QRect fullRect = event->rect().adjusted(labelFrameWidth / 2, labelFrameWidth / 2, -labelFrameWidth / 2, -labelFrameWidth / 2);

        if (pixmapExists) {
            painter.setOpacity(0.01 * m_opacityPercent);
            QRect pix = pixmap()->rect();
            int x = fullRect.left() + (fullRect.width() - pix.width()) / 2;
            int y = fullRect.top() + (fullRect.height() - pix.height()) / 2;
            painter.drawPixmap(x, y, *pixmap());
        }

        painter.setOpacity(0.5);
        QPen pen;
        pen.setWidth(labelFrameWidth);
        pen.setColor(QColor(Qt::lightGray));
        painter.setPen(pen);
        painter.drawRect(fullRect);
    }

private:
    int m_opacityPercent;
};

QnLayoutSettingsDialog::QnLayoutSettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QnLayoutSettingsDialog),
    m_cache(NULL),
    m_cellAspectRatio(qnGlobals->defaultLayoutCellAspectRatio()),
    m_newImageLoaded(false)
{
    ui->setupUi(this);

    imageLabel = new QnFramedLabel(ui->page);
    imageLabel->setObjectName(QLatin1String("imageLabel"));
    imageLabel->setText(tr("<No image>"));
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setOpacityPercent(ui->opacitySpinBox->value());
    ui->horizontalLayout->insertWidget(0, imageLabel, 1);

    imageLabel->installEventFilter(this);

    ui->userCanEditCheckBox->setVisible(false);
    ui->widthSpinBox->setMaximum(qnGlobals->layoutBackgroundMaxSize().width());
    ui->heightSpinBox->setMaximum(qnGlobals->layoutBackgroundMaxSize().height());
    ui->cropToMonitorCheckBox->setVisible(false);

    setProgress(false);

    connect(ui->viewButton,     SIGNAL(clicked()), this, SLOT(viewFile()));
    connect(ui->selectButton,   SIGNAL(clicked()), this, SLOT(selectFile()));
    connect(ui->clearButton,    SIGNAL(clicked()), this, SLOT(at_clearButton_clicked()));
    connect(ui->lockedCheckBox, SIGNAL(clicked()), this, SLOT(updateControls()));
    connect(ui->buttonBox,      SIGNAL(accepted()),this, SLOT(at_accepted()));
    connect(ui->opacitySpinBox, SIGNAL(valueChanged(int)), this, SLOT(at_opacitySpinBox_valueChanged(int)));
    connect(ui->cropToMonitorCheckBox, SIGNAL(toggled(bool)), this, SLOT(updatePreviewImageLabel()));

    updateControls();
}

QnLayoutSettingsDialog::~QnLayoutSettingsDialog()
{
}

void QnLayoutSettingsDialog::showEvent(QShowEvent *event) {
    base_type::showEvent(event);
    loadPreview();
}

void QnLayoutSettingsDialog::resizeEvent(QResizeEvent *event) {
    base_type::resizeEvent(event);
//    loadPreview();
}

bool QnLayoutSettingsDialog::eventFilter(QObject *target, QEvent *event) {
    if (target == imageLabel &&
            event->type() == QEvent::MouseButtonRelease) {
        if (!m_newFilePath.isEmpty())
            viewFile();
        else if (!ui->lockedCheckBox->isChecked())
            selectFile();
    }
    return base_type::eventFilter(target, event);
}

void QnLayoutSettingsDialog::readFromResource(const QnLayoutResourcePtr &layout) {
    m_cachedFilename = layout->backgroundImageFilename();

    updateCache(layout->hasFlags(QnResource::url | QnResource::local | QnResource::layout));

    if (!m_cachedFilename.isEmpty()) {
        m_newFilePath = m_cache->getFullPath(m_cachedFilename);
        m_cache->downloadFile(m_cachedFilename);
        ui->widthSpinBox->setValue(layout->backgroundSize().width());
        ui->heightSpinBox->setValue(layout->backgroundSize().height());
        ui->opacitySpinBox->setValue(layout->backgroundOpacity() * 100);
    }
    ui->lockedCheckBox->setChecked(layout->locked());
    ui->userCanEditCheckBox->setChecked(layout->userCanEdit());

    if (layout->cellAspectRatio() > 0) {
        qreal cellWidth = 1.0 + layout->cellSpacing().width();
        qreal cellHeight = 1.0 / layout->cellAspectRatio() + layout->cellSpacing().height();
        m_cellAspectRatio = cellWidth / cellHeight;
    }

    updateControls();
}

bool QnLayoutSettingsDialog::submitToResource(const QnLayoutResourcePtr &layout) {
    if (!hasChanges(layout))
        return false;

    layout->setUserCanEdit(ui->userCanEditCheckBox->isChecked());
    layout->setLocked(ui->lockedCheckBox->isChecked());
    layout->setBackgroundImageFilename(m_cachedFilename);
    layout->setBackgroundSize(QSize(ui->widthSpinBox->value(), ui->heightSpinBox->value()));
    layout->setBackgroundOpacity((qreal)ui->opacitySpinBox->value() * 0.01);
    // TODO: #GDM remove unused image if any
    return true;
}

qreal QnLayoutSettingsDialog::screenAspectRatio() const {
    QRect screen = qApp->desktop()->screenGeometry();
    return (qreal)screen.width() / (qreal)screen.height();
}

bool QnLayoutSettingsDialog::hasChanges(const QnLayoutResourcePtr &layout) {

    if (
            (ui->userCanEditCheckBox->isChecked() != layout->userCanEdit()) ||
            (ui->lockedCheckBox->isChecked() != layout->locked()) ||
            (ui->opacitySpinBox->value() != int(layout->backgroundOpacity() * 100)) ||
            (m_cachedFilename != layout->backgroundImageFilename())
            )
        return true;

    // do not save size change if no image was set
    QSize newSize(ui->widthSpinBox->value(), ui->heightSpinBox->value());
    return (!m_cachedFilename.isEmpty() && newSize != layout->backgroundSize());
}

void QnLayoutSettingsDialog::updateCache(bool local) {
    if (m_cache && ((dynamic_cast<QnLocalFileCache *>(m_cache) != NULL) == local))
        return;

    if (m_cache)
        disconnect(m_cache, 0, this, 0);

    m_cache = local
            ? new QnLocalFileCache(this)
            : new QnAppServerImageCache(this);

    connect(m_cache, SIGNAL(fileDownloaded(QString, bool)), this, SLOT(at_imageLoaded(QString, bool)));
    connect(m_cache, SIGNAL(fileUploaded(QString, bool)), this, SLOT(at_imageStored(QString, bool)));
}


void QnLayoutSettingsDialog::updateControls() {
    bool imagePresent = !m_newFilePath.isEmpty();
    bool locked = ui->lockedCheckBox->isChecked();

    ui->widthSpinBox->setEnabled(imagePresent && !locked);
    ui->heightSpinBox->setEnabled(imagePresent && !locked);
    ui->viewButton->setEnabled(imagePresent);
    ui->clearButton->setEnabled(imagePresent && !locked);
    ui->opacitySpinBox->setEnabled(imagePresent && !locked);
    ui->selectButton->setEnabled(!locked);
}

void QnLayoutSettingsDialog::at_clearButton_clicked() {
    m_newFilePath = QString();
    m_cachedFilename = QString();

    imageLabel->setPixmap(QPixmap());
    imageLabel->setText(tr("<No image>"));

    updateControls();
}

void QnLayoutSettingsDialog::at_accepted() {
    if (m_newFilePath.isEmpty() || !m_cachedFilename.isEmpty()) {
        accept();
        return;
    }

    if (ui->cropToMonitorCheckBox->isChecked())
        m_cache->storeImage(m_newFilePath, screenAspectRatio());
    else
        m_cache->storeImage(m_newFilePath);
    setProgress(true);
    ui->generalGroupBox->setEnabled(false);
    ui->buttonBox->setEnabled(false);
}

void QnLayoutSettingsDialog::at_opacitySpinBox_valueChanged(int value) {
    imageLabel->setOpacityPercent(value);
}

void QnLayoutSettingsDialog::at_imageLoaded(const QString &filename, bool ok) {
    if (m_cache->getFullPath(filename) != m_newFilePath)
        return;
    if (!ok) {
        imageLabel->setText(tr("<Image cannot be loaded>"));
        return;
    }
    loadPreview();
}

void QnLayoutSettingsDialog::at_imageStored(const QString &filename, bool ok) {
    setProgress(false);
    if (!ok) {
        imageLabel->setPixmap(QPixmap());
        imageLabel->setText(tr("<Image cannot be uploaded>"));
        m_cachedFilename = QString();
        m_newFilePath = QString();
        ui->generalGroupBox->setEnabled(true);
        ui->buttonBox->setEnabled(true);
        return;
    }

    m_cachedFilename = filename;
    accept();
}

void QnLayoutSettingsDialog::loadPreview() {
    if (!this->isVisible())
        return;

    imageLabel->setPixmap(QPixmap());
    imageLabel->setText(tr("<No image>"));
    if (m_newFilePath.isEmpty())
        return;

    QnThreadedImageLoader* loader = new QnThreadedImageLoader(this);
    loader->setInput(m_newFilePath);
    loader->setTransformationMode(Qt::FastTransformation);
    loader->setSize(imageLabel->size());
    loader->setFlags(Qn::TouchSizeFromOutside);
    connect(loader, SIGNAL(finished(QImage)), this, SLOT(setPreview(QImage)));
    loader->start();
    setProgress(true);
}

void QnLayoutSettingsDialog::viewFile() {
    QString path = QLatin1String("file:///") + m_newFilePath;
    if (QDesktopServices::openUrl(QUrl(path)))
        return;

    QnImagePreviewDialog dialog;
    dialog.openImage(m_newFilePath);
    dialog.exec();
}

void QnLayoutSettingsDialog::selectFile() {
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

    m_newFilePath = files[0];
    m_cachedFilename = QString();
    m_previewImage = QImage();
    m_previewCropped = QImage();
    m_estimatePending = true;
    ui->cropToMonitorCheckBox->setChecked(cropImage);

    loadPreview();
    updateControls();
}

void QnLayoutSettingsDialog::setPreview(const QImage &image) {
    setProgress(false);
    if (image.isNull()) {
        imageLabel->setPixmap(QPixmap());
        imageLabel->setText(tr("<Image cannot be loaded>"));
        return;
    }

    m_previewImage = image.scaled(imageLabel->size(), Qt::KeepAspectRatio, Qt::FastTransformation);
    m_previewCropped = cropImageToAspectRatio(image, screenAspectRatio());
    updatePreviewImageLabel();

    ui->cropToMonitorCheckBox->setVisible(true); //new image is selected



}

void QnLayoutSettingsDialog::setProgress(bool value) {
    ui->stackedWidget->setCurrentIndex(value ? 1 : 0);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!value);
    if (!value)
        updateControls();
}

void QnLayoutSettingsDialog::updatePreviewImageLabel() {
    QImage image = ui->cropToMonitorCheckBox->isChecked()
            ? m_previewCropped
            : m_previewImage;
    imageLabel->setPixmap(QPixmap::fromImage(image));

    if (!m_newImageLoaded)
        return;

    qreal aspectRatio = (qreal)image.width() / (qreal)image.height();

    int w, h;
    qreal targetAspectRatio = aspectRatio / m_cellAspectRatio;
    if (targetAspectRatio >= 1.0) { // width is greater than height
        w = qnGlobals->layoutBackgroundMaxSize().width();
        h = qRound((qreal)w / targetAspectRatio);
    } else {
        h = qnGlobals->layoutBackgroundMaxSize().height();
        w = qRound((qreal)h * targetAspectRatio);
    }

    // limit w*h <= recommended area; minor variations are allowed, e.g. 17*6 ~~= 100;
    qreal areaCoef = qSqrt((qreal)w * h / qnGlobals->layoutBackgroundRecommendedArea());
    if (areaCoef > 1.0) {
        w = qRound((qreal)w / areaCoef);
        h = qRound((qreal)h / areaCoef);
    }
    ui->widthSpinBox->setValue(w);
    ui->heightSpinBox->setValue(h);
}
