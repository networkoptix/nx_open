#include "local_files_messages.h"

#include <QtCore/QFileInfo>

#include <ui/dialogs/common/message_box.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace messages {

void LocalFiles::pathInvalid(QWidget* parent, const QString& path)
{
    QnMessageBox::warning(parent,
        tr("Path \"%1\" is invalid. Please try another path.")
        .arg(path));
}

void LocalFiles::fileExists(QWidget* parent, const QString& filename)
{
    QnMessageBox::warning(parent,
        tr("File \"%1\" already exists. Please try another name.")
        .arg(QFileInfo(filename).completeBaseName()));
}

void LocalFiles::fileIsBusy(QWidget* parent, const QString& filename)
{
    QnMessageBox::warning(parent,
        tr("File \"%1\" is used by another process. Please try another name.")
        .arg(QFileInfo(filename).completeBaseName()));
}

} // namespace messages
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx