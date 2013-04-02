#include "image_preview_dialog.h"
#include "ui_image_preview_dialog.h"

#include <utils/threaded_image_loader.h>

QnImagePreviewDialog::QnImagePreviewDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QnImagePreviewDialog)
{
    ui->setupUi(this);
}

QnImagePreviewDialog::~QnImagePreviewDialog()
{
}

void QnImagePreviewDialog::openImage(const QString &filename) {
    QnThreadedImageLoader* loader = new QnThreadedImageLoader(this);
    loader->setInput(filename);
    connect(loader, SIGNAL(finished(QImage)), this, SLOT(setImage(QImage)));
    loader->start();
}

void QnImagePreviewDialog::setImage(const QImage &image) {
    ui->imageLabel->setPixmap(QPixmap::fromImage(image));
}
