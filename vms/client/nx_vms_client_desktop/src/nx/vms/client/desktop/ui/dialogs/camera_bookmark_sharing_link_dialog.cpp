// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_bookmark_sharing_link_dialog.h"
#include "ui_camera_bookmark_sharing_link_dialog.h"

#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/bookmarks/bookmark_utils.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/text/human_readable.h>
#include <ui/common/palette.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::desktop {

struct CameraBookmarkSharingLinkDialog::Private
{
    CameraBookmarkSharingLinkDialog* const q;

    void setupControlsStyle() const;

    void setupControlsAvailability(
        const QnVirtualCameraResourcePtr& camera) const;

    void setupBookmarkDisplay(
        const common::CameraBookmark& bookmark,
        const QnVirtualCameraResourcePtr& camera) const;
};

void CameraBookmarkSharingLinkDialog::Private::setupControlsStyle() const
{
    static const core::SvgIconColorer::ThemeSubstitutions kStopSharingIconColor = {
        {QnIcon::Normal, {.primary = "light16"}}};

    q->ui->buttonsLayout->setContentsMargins(nx::style::Metrics::kDefaultTopLevelMargins);
    q->ui->buttonsLayout->setSpacing(nx::style::Metrics::kDefaultLayoutSpacing.width());

    setAccentStyle(q->ui->closeButton);

    q->ui->stopSharingButton->setIcon(
        qnSkin->icon("20x20/Solid/disabled.svg", kStopSharingIconColor));

    setPaletteColor(q->ui->urlLineEdit, QPalette::Text, core::colorTheme()->color("light16"));
}

void CameraBookmarkSharingLinkDialog::Private::setupControlsAvailability(
    const QnVirtualCameraResourcePtr& camera) const
{
    const bool canManageBookmarks =
        [camera]()
        {
            const auto cameraSystemContext =
                nx::vms::client::core::SystemContext::fromResource(camera);

            if (!NX_ASSERT(cameraSystemContext))
                return false;

            return cameraSystemContext->resourceAccessManager()->hasAccessRights(
                cameraSystemContext->userWatcher()->user(),
                camera,
                api::AccessRight::manageBookmarks);
        }();

    q->ui->editButton->setVisible(canManageBookmarks);
    q->ui->stopSharingButton->setVisible(canManageBookmarks);
}

void CameraBookmarkSharingLinkDialog::Private::setupBookmarkDisplay(
    const common::CameraBookmark& bookmark,
    const QnVirtualCameraResourcePtr& camera) const
{
    const auto hasExpiration = bookmark.bookmarkMatchesFilter(
        api::BookmarkShareFilter::shared | api::BookmarkShareFilter::hasExpiration);

    const auto expired = bookmark.bookmarkMatchesFilter(
        api::BookmarkShareFilter::shared | api::BookmarkShareFilter::expired);

    const auto accessible = bookmark.bookmarkMatchesFilter(
        api::BookmarkShareFilter::shared | api::BookmarkShareFilter::accessible);

    const auto passwordProtected = bookmark.bookmarkMatchesFilter(
        api::BookmarkShareFilter::shared | api::BookmarkShareFilter::passwordProtected);

    if (!hasExpiration)
    {
        q->ui->expirationTimeLabel->setText(tr("Never expires"));
    }
    else if (expired)
    {
        q->ui->expirationTimeLabel->setText(tr("Expired"));
    }
    else if (accessible)
    {
        q->ui->expirationTimeLabel->setText(tr("Expires in %1").arg(
            text::HumanReadable::timeSpan(bookmark.share.expirationTimeMs - qnSyncTime->value())));
    }

    q->ui->passwordInfoLabel->setText(passwordProtected
        ? tr("Password protected")
        : tr("No password protection"));

    const auto cameraSystemContext = nx::vms::client::core::SystemContext::fromResource(camera);
    if (!NX_ASSERT(cameraSystemContext))
        return;

    if (const auto sharingUrl =
        core::bookmarks::getBookmarkSharingLink(bookmark, cameraSystemContext))
    {
        q->ui->urlLineEdit->setText(sharingUrl->toString());
        q->ui->urlLineEdit->setCursorPosition(0);
    }
}

CameraBookmarkSharingLinkDialog::CameraBookmarkSharingLinkDialog(
    const common::CameraBookmark& bookmark,
    const QnVirtualCameraResourcePtr& camera,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::CameraBookmarkSharingLinkDialog),
    d(new Private{.q = this})

{
    ui->setupUi(this);

    d->setupControlsStyle();

    if (NX_ASSERT(bookmark.isValid() && !camera.isNull()))
    {
        d->setupControlsAvailability(camera);
        d->setupBookmarkDisplay(bookmark, camera);
    }

    connect(ui->closeButton, &QPushButton::clicked, this, &base_type::accept);

    connect(ui->editButton, &QPushButton::clicked, this,
        [this]()
        {
            base_type::accept();
            emit editClicked();
        });

    connect(ui->stopSharingButton, &QPushButton::clicked, this,
        [this]()
        {
            base_type::accept();
            emit stopSharingClicked();
        });

    connect(ui->copyToClipboardButton, &ClipboardButton::clicked, this,
        [this]() { ui->copyToClipboardButton->setClipboardText(ui->urlLineEdit->text()); });
}

CameraBookmarkSharingLinkDialog::~CameraBookmarkSharingLinkDialog()
{
}

} // namespace nx::vms::client::desktop
