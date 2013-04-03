#include "layout_settings_dialog.h"
#include "ui_layout_settings_dialog.h"

#include <QtGui/QFileDialog>

#include <core/resource/layout_resource.h>

#include <ui/dialogs/image_preview_dialog.h>

#include <utils/threaded_image_loader.h>

QnLayoutSettingsDialog::QnLayoutSettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QnLayoutSettingsDialog),
    m_cache(new QnAppServerFileCache(this)),
    m_layoutImageId(0)
{
    ui->setupUi(this);
    ui->userCanEditCheckBox->setVisible(false);

    setProgress(false);

    connect(ui->viewButton,     SIGNAL(clicked()), this, SLOT(at_viewButton_clicked()));
    connect(ui->selectButton,   SIGNAL(clicked()), this, SLOT(at_selectButton_clicked()));
    connect(ui->clearButton,    SIGNAL(clicked()), this, SLOT(at_clearButton_clicked()));
    connect(ui->lockedCheckBox, SIGNAL(clicked()), this, SLOT(updateControls()));
    connect(ui->buttonBox,      SIGNAL(accepted()),this, SLOT(at_accepted()));

    connect(m_cache, SIGNAL(imageLoaded(int)), this, SLOT(at_image_loaded(int)));
    connect(m_cache, SIGNAL(imageStored(int)), this, SLOT(at_image_stored(int)));

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

void QnLayoutSettingsDialog::readFromResource(const QnLayoutResourcePtr &layout) {
    m_layoutImageId = layout->backgroundImageId();
    if (m_layoutImageId > 0) {
        m_filename = m_cache->getPath(m_layoutImageId);
        m_cache->loadImage(m_layoutImageId);
        ui->widthSpinBox->setValue(layout->backgroundSize().width());
        ui->heightSpinBox->setValue(layout->backgroundSize().height());
        ui->opacitySpinBox->setValue(layout->backgroundOpacity());
    }
    ui->lockedCheckBox->setChecked(layout->locked());
    ui->userCanEditCheckBox->setChecked(layout->userCanEdit());
    updateControls();
}

bool QnLayoutSettingsDialog::submitToResource(const QnLayoutResourcePtr &layout) {
    if (!hasChanges(layout))
        return false;

    layout->setUserCanEdit(ui->userCanEditCheckBox->isChecked());
    layout->setLocked(ui->lockedCheckBox->isChecked());
    layout->setBackgroundImageId(m_layoutImageId);
    layout->setBackgroundSize(QSize(ui->widthSpinBox->value(), ui->heightSpinBox->value()));
    layout->setBackgroundOpacity(ui->opacitySpinBox->value());

    // TODO: progress dialog uploading image?
    // TODO: remove unused image if any

    return true;
}

bool QnLayoutSettingsDialog::hasChanges(const QnLayoutResourcePtr &layout) {

    if (
            (ui->userCanEditCheckBox->isChecked() != layout->userCanEdit()) ||
            (ui->lockedCheckBox->isChecked() != layout->locked()) ||
            (ui->opacitySpinBox->value() != layout->backgroundOpacity())
            )
        return true;

    // do not save size change if no image was set
    if (layout->backgroundImageId() == 0 && m_layoutImageId == 0)
        return false;
    QSize newSize(ui->widthSpinBox->value(), ui->heightSpinBox->value());
    return (m_layoutImageId != layout->backgroundImageId() || newSize != layout->backgroundSize());
}

void QnLayoutSettingsDialog::updateControls() {
    bool imagePresent = !m_filename.isEmpty() || m_layoutImageId > 0;
    bool locked = ui->lockedCheckBox->isChecked();

    ui->widthSpinBox->setEnabled(imagePresent && !locked);
    ui->heightSpinBox->setEnabled(imagePresent && !locked);
    ui->viewButton->setEnabled(imagePresent);
    ui->clearButton->setEnabled(imagePresent && !locked);
    ui->opacitySpinBox->setEnabled(imagePresent && !locked);
    ui->selectButton->setEnabled(!locked);
}

void QnLayoutSettingsDialog::at_viewButton_clicked() {
    QnImagePreviewDialog dialog;
    dialog.openImage(m_filename);
    dialog.exec();
}

void QnLayoutSettingsDialog::at_selectButton_clicked() {
    QScopedPointer<QFileDialog> dialog(new QFileDialog(this, tr("Open file")));
    dialog->setOption(QFileDialog::DontUseNativeDialog, true);
    dialog->setFileMode(QFileDialog::ExistingFile);
    dialog->setNameFilter(tr("Pictures (*.jpg *.png *.gif *.bmp *.tiff)"));

    if(!dialog->exec())
        return;

    QStringList files = dialog->selectedFiles();
    if (files.size() < 0)
        return;

    m_filename = files[0];
    m_layoutImageId = 0;

    loadPreview();
    updateControls();
}

void QnLayoutSettingsDialog::at_clearButton_clicked() {
    m_filename = QString();
    m_layoutImageId = 0;

    //TODO: #GDM special icon with "No image" text or may be complete help on the theme.
    ui->imageLabel->setPixmap(QPixmap(QLatin1String(":/skin/tree/snapshot.png")));
//    ui->estimateLabel->setText(QString());

    updateControls();
}

void QnLayoutSettingsDialog::at_accepted() {
    if (m_filename.isEmpty()) {
        accept();
        return;
    }

    m_cache->storeImage(m_filename);
    setProgress(true);
    ui->generalGroupBox->setEnabled(false);
    ui->buttonBox->setEnabled(false);
}

void QnLayoutSettingsDialog::at_image_loaded(int id) {
    if (m_cache->getPath(id) != m_filename)
        return;
    loadPreview();
}

void QnLayoutSettingsDialog::at_image_stored(int id) {
    setProgress(false);
    m_layoutImageId = id;
    accept();
}

void QnLayoutSettingsDialog::loadPreview() {
    if (!this->isVisible())
        return;

    ui->imageLabel->setPixmap(QPixmap(QLatin1String(":/skin/tree/snapshot.png")));

    QnThreadedImageLoader* loader = new QnThreadedImageLoader(this);
    loader->setInput(m_filename);
    loader->setTransformationMode(Qt::FastTransformation);
    loader->setSize(ui->imageLabel->size());
    connect(loader, SIGNAL(finished(QImage)), this, SLOT(setPreview(QImage)));
    loader->start();
    setProgress(true);
}

void QnLayoutSettingsDialog::setPreview(const QImage &image) {
    setProgress(false);
    if (image.isNull())
        return;

    ui->imageLabel->setPixmap(QPixmap::fromImage(image));

 /*   qreal aspectRatio = (qreal)image.width() / (qreal)image.height();
    int w, h;

    qreal point = (qreal)ui->widthSpinBox->maximum() / (qreal)ui->heightSpinBox->maximum();
    if (aspectRatio >= point) {
        w = ui->widthSpinBox->maximum();
        h = qRound((qreal)w / aspectRatio);
    } else {
        h = ui->heightSpinBox->maximum();
        w = qRound((qreal)h * aspectRatio);
    }
    ui->estimateLabel->setText(tr("Aspect Ratio: %1x%2").arg(w).arg(h));*/
}

void QnLayoutSettingsDialog::setProgress(bool value) {
    ui->stackedWidget->setCurrentIndex(value ? 1 : 0);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!value);
    if (!value)
        updateControls();
}

