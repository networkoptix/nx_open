#include "videowall_messages.h"

#include <ui/dialogs/common/message_box.h>

#include <client/client_app_info.h>

namespace nx {
namespace client {
namespace messages {

void VideoWall::anotherVideoWall(QWidget* parent)
{
    QnMessageBox::warning(parent, tr("There is another video wall with the same name"));
}

bool VideoWall::switchToVideoWallMode(QWidget* parent, bool* closeCurrentInstanse)
{
    QnMessageBox dialog(QnMessageBoxIcon::Question,
        tr("Close %1 before starting Video Wall?").arg(QnClientAppInfo::applicationDisplayName()),
        QString(),
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton,
        parent);

    const auto closeButton =
        dialog.addButton(tr("Close"), QDialogButtonBox::YesRole, QnButtonAccent::Standard);
    dialog.addButton(tr("Keep"), QDialogButtonBox::NoRole);

    const auto result = dialog.exec();

    if (closeCurrentInstanse)
        *closeCurrentInstanse = dialog.clickedButton() == closeButton;

    return result != QDialogButtonBox::Cancel;
}

void VideoWall::localFilesForbidden(QWidget* parent)
{
    QnMessageBox::warning(parent,
        tr("Local files can't be placed on Video Wall Screen attached to another computer"),
        tr("To display local files on the Video Wall, please attach them using computer where Video Wall is hosted.")
        );
}

} // namespace messages
} // namespace client
} // namespace nx
