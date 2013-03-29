#include "layout_settings_dialog.h"
#include "ui_layout_settings_dialog.h"

#include <QtGui/QFileDialog>

#include <core/resource/layout_resource.h>

QnLayoutSettingsDialog::QnLayoutSettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QnLayoutSettingsDialog),
    m_cache(new QnAppServerFileCache(this)),
    m_imageId(0)
{
    ui->setupUi(this);

    connect(ui->viewButton,     SIGNAL(clicked()), this, SLOT(at_viewButton_clicked()));
    connect(ui->selectButton,   SIGNAL(clicked()), this, SLOT(at_selectButton_clicked()));
    connect(ui->clearButton,    SIGNAL(clicked()), this, SLOT(at_clearButton_clicked()));
    connect(ui->lockedCheckBox, SIGNAL(clicked()), this, SLOT(updateControls()));

    connect(m_cache, SIGNAL(imageLoaded(int,QImage)), this, SLOT(at_image_loaded(int, const QImage &)));

    updateControls();
}

QnLayoutSettingsDialog::~QnLayoutSettingsDialog()
{
}

void QnLayoutSettingsDialog::readFromResource(const QnLayoutResourcePtr &layout) {

    m_imageId = layout->backgroundImageId();
    ui->widthSpinBox->setValue(layout->backgroundSize().width());
    ui->heightSpinBox->setValue(layout->backgroundSize().height());
    ui->lockedCheckBox->setChecked(layout->locked());

    ui->userCanEditCheckBox->setChecked(layout->userCanEdit());
    updateControls();
}

bool QnLayoutSettingsDialog::submitToResource(const QnLayoutResourcePtr &layout) {
    if (!hasChanges(layout))
        return false;

    layout->setUserCanEdit(ui->userCanEditCheckBox->isChecked());
    layout->setLocked(ui->lockedCheckBox->isChecked());
    layout->setBackgroundImageId(m_imageId);
    layout->setBackgroundSize(QSize(ui->widthSpinBox->value(), ui->heightSpinBox->value()));

    // TODO: progress dialog uploading image?
    // TODO: remove unused image if any

    return true;
}

bool QnLayoutSettingsDialog::hasChanges(const QnLayoutResourcePtr &layout) {

    if (ui->userCanEditCheckBox->isChecked() != layout->userCanEdit())
        return true;

    if (ui->lockedCheckBox->isChecked() != layout->locked())
        return true;

    if (layout->backgroundImageId() == 0 && m_imageId == 0)
        return false;

    QSize newSize(ui->widthSpinBox->value(), ui->heightSpinBox->value());
    return (m_imageId != layout->backgroundImageId() || newSize != layout->backgroundSize());
}

void QnLayoutSettingsDialog::updateControls() {
    bool imagePresent = m_imageId > 0;
    bool locked = ui->lockedCheckBox->isChecked();

    ui->widthSpinBox->setEnabled(imagePresent && !locked);
    ui->heightSpinBox->setEnabled(imagePresent && !locked);
    ui->viewButton->setEnabled(imagePresent);
    ui->clearButton->setEnabled(imagePresent && !locked);
    ui->selectButton->setEnabled(!locked);
}

void QnLayoutSettingsDialog::at_viewButton_clicked() {

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

    QPixmap image(files[0]);
    ui->imageLabel->setPixmap(image.scaled(ui->imageLabel->size(), Qt::KeepAspectRatio));
    qreal aspectRatio = (qreal)image.width() / (qreal)image.height();
    if (aspectRatio >= 1.0) {
        ui->widthSpinBox->setValue(ui->widthSpinBox->maximum());
        ui->heightSpinBox->setValue(qRound((qreal)ui->widthSpinBox->value() / aspectRatio));
    } else {
        ui->heightSpinBox->setValue(ui->heightSpinBox->maximum());
        ui->widthSpinBox->setValue(qRound((qreal)ui->heightSpinBox->value() * aspectRatio));
    }

    //TODO: #GDM replace with uploading
    m_imageId = m_cache->appendDebug(files[0]);
    updateControls();
}

void QnLayoutSettingsDialog::at_clearButton_clicked() {
    m_imageId = 0;
    //TODO: #GDM special icon with "No image" text or may be complete help on the theme.
    ui->imageLabel->setPixmap(QPixmap(QLatin1String(":/skin/tree/snapshot.png")));

    updateControls();
}

void QnLayoutSettingsDialog::at_image_loaded(int id, const QImage &image) {
    if (id != m_imageId)
        return;
    QPixmap pixmap;
    if (!pixmap.convertFromImage(image.scaled(ui->imageLabel->size(), Qt::KeepAspectRatio)))
        qWarning() << "Could not convert image";
    ui->imageLabel->setPixmap(pixmap);
}




