#include "file_messages.h"

#include <ui/dialogs/common/message_box.h>

bool QnFileMessages::confirmOverwrite(
    QWidget* parent,
    const QString& fileName)
{
    QnMessageBox dialog(QnMessageBoxIcon::Question,
        tr("Overwrite existing file?"), fileName,
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton, parent);

    dialog.addCustomButton(QnMessageBoxCustomButton::Overwrite,
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);
    return (dialog.exec() != QDialogButtonBox::Cancel);
}

void QnFileMessages::overwriteFailed(
    QWidget* parent,
    const QString& fileName)
{
    QnMessageBox::critical(parent, tr("Failed to overwrite file"), fileName);
}
