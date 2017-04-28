#include "camera_bookmark_dialog.h"
#include "ui_camera_bookmark_dialog.h"

#include <QtWidgets/QPushButton>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

QnCameraBookmarkDialog::QnCameraBookmarkDialog(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnCameraBookmarkDialog)
{
    ui->setupUi(this);
    setHelpTopic(this, Qn::Bookmarks_Editing_Help);

    connect(ui->bookmarkWidget, &QnBookmarkWidget::validChanged, this, [this] {
        if (!buttonBox())
            return;
        auto okButton = buttonBox()->button(QDialogButtonBox::Ok);
        if (!okButton)
            return;
        okButton->setEnabled(ui->bookmarkWidget->isValid());
    });
}

QnCameraBookmarkDialog::~QnCameraBookmarkDialog() {}

const QnCameraBookmarkTagList &QnCameraBookmarkDialog::tags() const {
    return ui->bookmarkWidget->tags();
}

void QnCameraBookmarkDialog::setTags(const QnCameraBookmarkTagList &tags) {
    ui->bookmarkWidget->setTags(tags);
}

void QnCameraBookmarkDialog::loadData(const QnCameraBookmark &bookmark) {
    ui->bookmarkWidget->loadData(bookmark);
}

void QnCameraBookmarkDialog::submitData(QnCameraBookmark &bookmark) const {
    ui->bookmarkWidget->submitData(bookmark);
}

void QnCameraBookmarkDialog::accept()
{
    if (!ui->bookmarkWidget->isValid())
        return;

    base_type::accept();
}
