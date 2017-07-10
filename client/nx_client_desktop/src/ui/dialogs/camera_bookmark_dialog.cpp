#include "camera_bookmark_dialog.h"
#include "ui_camera_bookmark_dialog.h"

#include <QtWidgets/QPushButton>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource_management/resource_pool.h>
#include <utils/camera/bookmark_helpers.h>

namespace {

QnCameraBookmarkList extractBookmarksFromAction(
    const nx::vms::event::AbstractActionPtr& action,
    QnResourcePool* resourcePool,
    QnCommonModule* commonModule)
{
    QnCameraBookmarkList bookmarks;

    const auto addBookmarkByResourceId =
        [action, commonModule, resourcePool, &bookmarks](const QnUuid& resourceId)
        {
            const auto camera = resourcePool->getResourceById<QnSecurityCamResource>(resourceId);
            if (!camera)
            {
                NX_EXPECT(false, "Invalid camera resource");
                return;
            }

            const auto bookmark = helpers::bookmarkFromAction(action, camera, commonModule);
            if (bookmark.isValid())
                bookmarks.append(bookmark);
            else
                NX_EXPECT(false, "Invalid bookmark");
        };

    // Extracts bookmarks from action-specified resources.
    for (const auto& resourceId: action->getResources())
        addBookmarkByResourceId(resourceId);

    // Extracts bookmark for single-camera event (show popup, for example).
    addBookmarkByResourceId(action->getRuntimeParams().eventResourceId);

    return bookmarks;
}

}

QnCameraBookmarkDialog::QnCameraBookmarkDialog(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnCameraBookmarkDialog)
{
    initialize();
}

QnCameraBookmarkDialog::QnCameraBookmarkDialog(
    const nx::vms::event::AbstractActionPtr& action,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::QnCameraBookmarkDialog()),
    m_actionBookmarks(extractBookmarksFromAction(action, resourcePool(), commonModule()))
{
    NX_EXPECT(!m_actionBookmarks.isEmpty());

    initialize();
    ui->bookmarkWidget->setDescriptionMandatory();
}

QnCameraBookmarkDialog::~QnCameraBookmarkDialog() {}

void QnCameraBookmarkDialog::initialize()
{
    ui->setupUi(this);
    setHelpTopic(this, Qn::Bookmarks_Editing_Help);

    connect(ui->bookmarkWidget, &QnBookmarkWidget::validChanged,
        this, &QnCameraBookmarkDialog::updateOkButtonAvailability);
    updateOkButtonAvailability();
}

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
