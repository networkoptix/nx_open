#include "camera_bookmark_dialog.h"
#include "ui_camera_bookmark_dialog.h"

QnCameraBookmarkDialog::QnCameraBookmarkDialog(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnCameraBookmarkDialog)
{
    ui->setupUi(this);
}

QnCameraBookmarkDialog::~QnCameraBookmarkDialog() {}

QnCameraBookmarkTags QnCameraBookmarkDialog::tags() const {
    return ui->bookmarkWidget->tags();
}

void QnCameraBookmarkDialog::setTags(const QnCameraBookmarkTags &tags) {
    ui->bookmarkWidget->setTags(tags);
}

void QnCameraBookmarkDialog::loadData(const QnCameraBookmark &bookmark) {
    ui->bookmarkWidget->loadData(bookmark);
}

void QnCameraBookmarkDialog::submitData(QnCameraBookmark &bookmark) const {
    ui->bookmarkWidget->submitData(bookmark);
}
