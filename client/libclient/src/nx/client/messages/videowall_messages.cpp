#include "videowall_messages.h"

#include <client/client_app_info.h>
#include <client/client_settings.h>

#include <core/resource/resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>

#include <ui/dialogs/common/message_box.h>

#include <utils/common/delayed.h>

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

bool VideoWall::checkLocalFiles(QWidget* parent,
    const QnVideoWallItemIndex& index,
    const QnResourceList& resources,
    bool displayDelayed)
{
    const bool itemBelongsToThisPc = index.item().pcUuid == qnSettings->pcUuid();
    if (itemBelongsToThisPc)
        return true;

    bool hasLocalFiles = boost::algorithm::any_of(resources,
        [](const QnResourcePtr& resource)
        {
            return resource->hasFlags(Qn::local_media);
        });

    if (!hasLocalFiles)
        return true;

    auto execMessage = [parent]
        {
            QnMessageBox::warning(parent,
                tr("Local files can't be placed on Video Wall Screen attached to another computer"),
                tr("To display local files on the Video Wall, please attach them using computer where Video Wall is hosted.")
            );
        };

    if (displayDelayed)
        executeDelayedParented(execMessage, kDefaultDelay, parent);
    else
        execMessage();

    return false;
}


} // namespace messages
} // namespace client
} // namespace nx
