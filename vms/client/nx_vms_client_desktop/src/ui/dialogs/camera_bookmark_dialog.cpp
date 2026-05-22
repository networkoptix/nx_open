// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_bookmark_dialog.h"
#include "ui_camera_bookmark_dialog.h"

#include <QtWidgets/QPushButton>

#include <core/resource/camera_bookmark.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/bookmarks/bookmark_utils.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>

using namespace nx::vms::client::desktop;

struct QnCameraBookmarkDialog::Private
{
    QnCameraBookmarkDialog* const q;

    Mode mode;
    QPointer<QPushButton> createAndShareButton;
    bool sharingRequested = false;
};

QnCameraBookmarkDialog::QnCameraBookmarkDialog(
    Mode mode,
    bool mandatoryDescription,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::QnCameraBookmarkDialog),
    d(new Private{.q = this, .mode = mode})
{
    ui->setupUi(this);
    setHelpTopic(this, HelpTopic::Id::Bookmarks_Editing);

    connect(ui->bookmarkWidget, &QnBookmarkWidget::validChanged,
        this, &QnCameraBookmarkDialog::updateAcceptButtonsAvailability);
    updateAcceptButtonsAvailability();

    ui->bookmarkWidget->setDescriptionMandatory(mandatoryDescription);
}

QnCameraBookmarkDialog::~QnCameraBookmarkDialog()
{
}

void QnCameraBookmarkDialog::initializeButtonBox()
{
    base_type::initializeButtonBox();

    if (d->mode == Mode::create)
    {
        const auto createButton = buttonBox()->addButton(
            tr("Create"), QDialogButtonBox::ButtonRole::AcceptRole);
        setAccentStyle(createButton);

        if (nx::vms::client::core::bookmarks::bookmarkSharingAvailable(systemContext()))
        {
            d->createAndShareButton = buttonBox()->addButton(
                tr("Create and Share..."), QDialogButtonBox::ButtonRole::AcceptRole);
        }
    }
    else
    {
        const auto okButton = buttonBox()->addButton(QDialogButtonBox::StandardButton::Ok);
        setAccentStyle(okButton);
    }

    buttonBox()->addButton(QDialogButtonBox::StandardButton::Cancel);

    updateAcceptButtonsAvailability();
}

void QnCameraBookmarkDialog::buttonBoxClicked(QAbstractButton* button)
{
    base_type::buttonBoxClicked(button);

    if (button == d->createAndShareButton)
        d->sharingRequested = true;
}

void QnCameraBookmarkDialog::updateAcceptButtonsAvailability()
{
    if (!buttonBox())
        return;

    const auto bookmarkValid = ui->bookmarkWidget->isValid();
    const auto buttonBoxButtons = buttonBox()->buttons();

    for (const auto button: buttonBoxButtons)
    {
        if (buttonBox()->buttonRole(button) == QDialogButtonBox::ButtonRole::AcceptRole)
            button->setEnabled(bookmarkValid);
    }
}

const QnCameraBookmarkTagList& QnCameraBookmarkDialog::tags() const
{
    return ui->bookmarkWidget->tags();
}

void QnCameraBookmarkDialog::setTags(const QnCameraBookmarkTagList& tags)
{
    ui->bookmarkWidget->setTags(tags);
}

void QnCameraBookmarkDialog::loadData(const QnCameraBookmark& bookmark)
{
    ui->bookmarkWidget->loadData(bookmark);
}

void QnCameraBookmarkDialog::submitData(QnCameraBookmark& bookmark) const
{
    ui->bookmarkWidget->submitData(bookmark);
    if (sharingRequested())
        bookmark.share = nx::vms::client::core::bookmarks::defaultBookmarkSharingParams();
}

bool QnCameraBookmarkDialog::sharingRequested() const
{
    return d->sharingRequested;
}

void QnCameraBookmarkDialog::accept()
{
    if (!ui->bookmarkWidget->isValid())
        return;

    base_type::accept();
}
