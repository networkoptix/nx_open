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
        .arg(QFileInfo(filename).fileName()));
}

void LocalFiles::fileIsBusy(QWidget* parent, const QString& filename)
{
    QnMessageBox::warning(parent,
        tr("File \"%1\" is used by another process.")
        .arg(QFileInfo(filename).fileName()));
}

void LocalFiles::fileCannotBeWritten(QWidget* parent, const QString& filename)
{
    QnMessageBox::warning(parent,
        tr("File \"%1\" cannot be written.")
        .arg(QFileInfo(filename).fileName()));
}

void LocalFiles::invalidChars(QWidget* parent, const QString& invalidSet)
{
    QnMessageBox::warning(parent,
        tr("Filename should not contain the following reserved characters:\n%1", "", invalidSet.size())
        .arg(invalidSet));
}

void LocalFiles::reservedFilename(QWidget* parent, const QString& filename)
{
    QnMessageBox::warning(parent,
        tr("Filename \"%1\" is reserved by operating system. Please try another name.")
        .arg(filename));
}

} // namespace messages
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx