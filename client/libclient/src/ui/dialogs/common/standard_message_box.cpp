#include "standard_message_box.h"

#include <ui/dialogs/common/message_box.h>

bool QnStandardMessageBox::overwriteFileQuestion(
    QWidget* parent,
    const QString& fileName)
{
    QnMessageBox dialog(QnMessageBoxIcon::Question,
        tr("Overwrite existing file?"), fileName,
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton, parent);

    dialog.addCustomButton(QnMessageBoxCustomButton::Overwrite);
    return (dialog.exec() != QDialogButtonBox::Cancel);
}

void QnStandardMessageBox::failedToOverwriteMessage(
    QWidget* parent,
    const QString& fileName)
{
    QnMessageBox::critical(parent, tr("Failed to overwrite file"), fileName);
}
