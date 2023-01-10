// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "videowall_messages.h"

#include <algorithm>

#include <QtWidgets/QWidget>
#include <QtWidgets/QPushButton>

#include <client/client_settings.h>

#include <core/resource/resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>

#include <ui/dialogs/common/message_box.h>

#include <utils/common/delayed.h>

#include <nx/branding.h>

namespace nx::vms::client::desktop {
namespace ui {
namespace messages {

void Videowall::anotherVideoWall(QWidget* parent)
{
    QnMessageBox::warning(parent, tr("There is another video wall with the same name"));
}

bool Videowall::switchToVideoWallMode(QWidget* parent, bool* closeCurrentInstanse)
{
    QnMessageBox dialog(QnMessageBoxIcon::Question,
        tr("Close %1 before starting Video Wall?").arg(nx::branding::desktopClientDisplayName()),
        QString(),
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton,
        parent);

    const auto closeButton =
        dialog.addButton(tr("Close"), QDialogButtonBox::YesRole, Qn::ButtonAccent::Standard);
    dialog.addButton(tr("Keep"), QDialogButtonBox::NoRole);

    const auto result = dialog.exec();

    if (closeCurrentInstanse)
        *closeCurrentInstanse = dialog.clickedButton() == closeButton;

    return result != QDialogButtonBox::Cancel;
}

bool Videowall::checkLocalFiles(QWidget* parent,
    const QnVideoWallItemIndex& index,
    const QnResourceList& resources,
    bool displayDelayed)
{
    const bool itemBelongsToThisPc = index.item().pcUuid == qnSettings->pcUuid();
    if (itemBelongsToThisPc)
        return true;

    bool hasLocalFiles = std::any_of(resources.begin(), resources.end(),
        [](const QnResourcePtr& resource)
        {
            return resource->hasFlags(Qn::local_media);
        });

    if (!hasLocalFiles)
        return true;

    auto execMessage = [parent]
        {
            QnMessageBox::warning(parent,
                tr("Local files cannot be placed on Video Wall Screen attached to another computer"),
                tr("To display local files on Video Wall, please attach them using computer where Video Wall is hosted.")
            );
        };

    if (displayDelayed)
        executeDelayedParented(execMessage, parent);
    else
        execMessage();

    return false;
}

} // namespace messages
} // namespace ui
} // namespace nx::vms::client::desktop
