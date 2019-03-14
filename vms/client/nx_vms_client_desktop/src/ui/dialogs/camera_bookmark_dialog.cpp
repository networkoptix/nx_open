#include "camera_bookmark_dialog.h"
#include "ui_camera_bookmark_dialog.h"

#include <QtWidgets/QPushButton>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource_management/resource_pool.h>

QnCameraBookmarkDialog::QnCameraBookmarkDialog(bool mandatoryDescription, QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnCameraBookmarkDialog)
{
    ui->setupUi(this);
    setHelpTopic(this, Qn::Bookmarks_Editing_Help);

    connect(ui->bookmarkWidget, &QnBookmarkWidget::validChanged,
        this, &QnCameraBookmarkDialog::updateOkButtonAvailability);
    updateOkButtonAvailability();

    ui->bookmarkWidget->setDescriptionMandatory(mandatoryDescription);
}

QnCameraBookmarkDialog::~QnCameraBookmarkDialog() {}

void QnCameraBookmarkDialog::initializeButtonBox()
{
    base_type::initializeButtonBox();
    updateOkButtonAvailability();
}

void QnCameraBookmarkDialog::updateOkButtonAvailability()
{
    const auto box = buttonBox();
    if (!box)
        return;

    if (auto okButton = box->button(QDialogButtonBox::Ok))
        okButton->setEnabled(ui->bookmarkWidget->isValid());
}

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
