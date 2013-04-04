#include "layout_settings_dialog.h"
#include "ui_layout_settings_dialog.h"

#include <QtCore/qmath.h>

#include <QtGui/QFileDialog>
#include <QtGui/QDesktopServices>
#include <QtGui/QPainter>
#include <QtGui/QPen>
#include <QtGui/QPaintEvent>

#include <core/resource/layout_resource.h>

#include <ui/dialogs/image_preview_dialog.h>

#include <utils/threaded_image_loader.h>

namespace {
    //limits
    const int widthLimit = 20;      // in cells
    const int heightLimit = 20;     // in cells
    const int areaLimit = 100;      // in cells

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
    m_cache(new QnAppServerFileCache(this)),
    m_layoutImageId(0),
    m_cellAspectRatio((qreal)16/9),
    m_estimatePending(false)
{
    ui->setupUi(this);

    imageLabel = new QnFramedLabel(ui->page);
    imageLabel->setObjectName(QLatin1String("imageLabel"));
    imageLabel->setText(tr("<No image>"));
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setOpacityPercent(ui->opacitySpinBox->value());
    ui->horizontalLayout->insertWidget(0, imageLabel, 1);

    ui->userCanEditCheckBox->setVisible(false);
    ui->widthSpinBox->setMaximum(widthLimit);
    ui->heightSpinBox->setMaximum(heightLimit);

    setProgress(false);

    connect(ui->viewButton,     SIGNAL(clicked()), this, SLOT(at_viewButton_clicked()));
    connect(ui->selectButton,   SIGNAL(clicked()), this, SLOT(at_selectButton_clicked()));
    connect(ui->clearButton,    SIGNAL(clicked()), this, SLOT(at_clearButton_clicked()));
    connect(ui->lockedCheckBox, SIGNAL(clicked()), this, SLOT(updateControls()));
    connect(ui->buttonBox,      SIGNAL(accepted()),this, SLOT(at_accepted()));
    connect(ui->opacitySpinBox, SIGNAL(valueChanged(int)), this, SLOT(at_opacitySpinBox_valueChanged(int)));

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

    qreal cellWidth = 1.0 + layout->cellSpacing().width();
    qreal cellHeight = 1.0 / layout->cellAspectRatio() + layout->cellSpacing().height();
    m_cellAspectRatio = cellWidth / cellHeight;

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
    QString path = QLatin1String("file:///") + m_filename;
    if (QDesktopServices::openUrl(QUrl(path)))
        return;

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
    m_estimatePending = true;

    loadPreview();
    updateControls();
}

void QnLayoutSettingsDialog::at_clearButton_clicked() {
    m_filename = QString();
    m_layoutImageId = 0;

    imageLabel->setPixmap(QPixmap());
    imageLabel->setText(tr("<No image>"));

    updateControls();
}

void QnLayoutSettingsDialog::at_accepted() {
    if (m_filename.isEmpty() || m_layoutImageId > 0) {
        accept();
        return;
    }

    m_cache->storeImage(m_filename);
    setProgress(true);
    ui->generalGroupBox->setEnabled(false);
    ui->buttonBox->setEnabled(false);
}

void QnLayoutSettingsDialog::at_opacitySpinBox_valueChanged(int value) {
    imageLabel->setOpacityPercent(value);
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

    imageLabel->setPixmap(QPixmap());
    imageLabel->setText(tr("<No image>"));
    if (m_filename.isEmpty())
        return;

    QnThreadedImageLoader* loader = new QnThreadedImageLoader(this);
    loader->setInput(m_filename);
    loader->setTransformationMode(Qt::FastTransformation);
    loader->setSize(imageLabel->size());
    connect(loader, SIGNAL(finished(QImage)), this, SLOT(setPreview(QImage)));
    loader->start();
    setProgress(true);
}

void QnLayoutSettingsDialog::setPreview(const QImage &image) {
    setProgress(false);
    if (image.isNull()) {
        imageLabel->setPixmap(QPixmap());
        imageLabel->setText(tr("<Image cannot be loaded>"));
        return;
    }
    imageLabel->setPixmap(QPixmap::fromImage(image));

    if (!m_estimatePending)
        return;
    m_estimatePending = false;

    qreal aspectRatio = (qreal)image.width() / (qreal)image.height();

    int w, h;
    qreal targetAspectRatio = aspectRatio / m_cellAspectRatio; // targetAspectRatio = w/h;
    if (targetAspectRatio >= 1.0) { // width is greater than height
        w = ui->widthSpinBox->maximum();
        h = qRound((qreal)w / targetAspectRatio);
    } else {
        h = ui->heightSpinBox->maximum();
        w = qRound((qreal)h * targetAspectRatio);
    }

    // limit w*h <= areaLimit; minor variations are available, e.g. 17*6 ~~= 100;
    qreal areaCoef = qSqrt((qreal)w * h / areaLimit);
    if (areaCoef > 1.0) {
        w = qRound((qreal)w / areaCoef);
        h = qRound((qreal)h / areaCoef);
    }
    ui->widthSpinBox->setValue(w);
    ui->heightSpinBox->setValue(h);

}

void QnLayoutSettingsDialog::setProgress(bool value) {
    ui->stackedWidget->setCurrentIndex(value ? 1 : 0);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!value);
    if (!value)
        updateControls();
}

