// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "image_preview_dialog.h"
#include "ui_image_preview_dialog.h"

#include <nx/vms/client/desktop/image_providers/threaded_image_loader.h>

using namespace nx::vms::client::desktop;

QnImagePreviewDialog::QnImagePreviewDialog(QWidget *parent) :
    base_type(parent),
    ui(new Ui::ImagePreviewDialog)
{
    ui->setupUi(this);
}

QnImagePreviewDialog::~QnImagePreviewDialog()
{
}

void QnImagePreviewDialog::openImage(const QString &filename)
{
    auto loader = new ThreadedImageLoader(this);
    loader->setInput(filename);
    connect(loader, &ThreadedImageLoader::imageLoaded, this, &QnImagePreviewDialog::setImage);
    connect(loader, &ThreadedImageLoader::imageLoaded, loader, &QObject::deleteLater);
    loader->start();
}

void QnImagePreviewDialog::setImage(const QImage& image)
{
    ui->stackedWidget->setCurrentIndex(0);
    ui->imageLabel->setPixmap(QPixmap::fromImage(image));
}
