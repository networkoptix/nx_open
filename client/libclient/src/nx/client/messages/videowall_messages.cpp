#include "videowall_messages.h"

#include <ui/dialogs/common/message_box.h>

namespace nx {
namespace client {
namespace messages {

void VideoWall::anotherVideoWall(QWidget* parent)
{
    QnMessageBox::warning(parent, tr("There is another video wall with the same name"));
}

} // namespace messages
} // namespace client
} // namespace nx
