#include "layout_settings_dialog.h"
#include "ui_layout_settings_dialog.h"

#include <QtGui/QFileDialog>

#include <core/resource/layout_resource.h>
#include <utils/threaded_image_loader.h>

QnLayoutSettingsDialog::QnLayoutSettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QnLayoutSettingsDialog),
    m_cache(new QnAppServerFileCache(this)),
    m_layoutImageId(0)
{
    ui->setupUi(this);
    ui->progressBar->setVisible(false);
    ui->estimateSizeButton->setEnabled(false);

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

void QnLayoutSettingsDialog::readFromResource(const QnLayoutResourcePtr &layout) {

    m_layoutImageId = layout->backgroundImageId();
    if (m_layoutImageId > 0) {
        m_filename = m_cache->getPath(m_layoutImageId);
        m_cache->loadImage(m_layoutImageId);
    }

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
    layout->setBackgroundImageId(m_layoutImageId);
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
    ui->selectButton->setEnabled(!locked);
}

void QnLayoutSettingsDialog::at_viewButton_clicked() {
/*
    QImage img = m_cache->getImage(m_imageId);
    if (img.isNull())
        return; //show message?

    QDialog d(this);

    QVBoxLayout *layout = new QVBoxLayout(&d);

    QLabel* l = new QLabel(&d);
    l->setPixmap(QPixmap::fromImage(img));
    layout->addWidget(l);

    QPushButton* b = new QPushButton(&d);
    b->setText(tr("Close"));
    connect(b, SIGNAL(clicked()), &d, SLOT(close()));
    layout->addWidget(b);

    d.setLayout(layout);
    d.setModal(true);
    d.exec();
*/
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
    ui->estimateSizeButton->setEnabled(false);

    updateControls();
}

void QnLayoutSettingsDialog::at_estimateSizeButton_clicked() {
    const QPixmap* pixmap = ui->imageLabel->pixmap();
    qreal aspectRatio = (qreal)pixmap->width() / (qreal)pixmap->height();
    if (aspectRatio >= 1.0) {
        ui->widthSpinBox->setValue(ui->widthSpinBox->maximum());
        ui->heightSpinBox->setValue(qRound((qreal)ui->widthSpinBox->value() / aspectRatio));
    } else {
        ui->heightSpinBox->setValue(ui->heightSpinBox->maximum());
        ui->widthSpinBox->setValue(qRound((qreal)ui->heightSpinBox->value() * aspectRatio));
    }
}

void QnLayoutSettingsDialog::at_accepted() {
    if (m_filename.isEmpty()) {
        accept();
        return;
    }

    m_cache->storeImage(m_filename);
    ui->progressBar->setVisible(true);
}

void QnLayoutSettingsDialog::at_image_loaded(int id) {
    if (m_cache->getPath(id) != m_filename)
        return;
    loadPreview();
}

void QnLayoutSettingsDialog::at_image_stored(int id) {
    ui->progressBar->setVisible(false);
    m_layoutImageId = id;
    accept();
}

void QnLayoutSettingsDialog::loadPreview() {
    if (!this->isVisible())
        return;

    QnThreadedImageLoader* loader = new QnThreadedImageLoader(this);
    loader->setInput(m_filename);
    loader->setTransformationMode(Qt::FastTransformation);
    loader->setSize(ui->imageLabel->size());
    connect(loader, SIGNAL(finished(QImage)), this, SLOT(setPreview(QImage)));
    loader->start();
    ui->progressBar->setVisible(true);
}

void QnLayoutSettingsDialog::setPreview(const QImage &image) {
    ui->progressBar->setVisible(false);
    if (image.isNull())
        return;

    ui->imageLabel->setPixmap(QPixmap::fromImage(image));
    ui->estimateSizeButton->setEnabled(true);
}


