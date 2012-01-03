#include "ui_common.h"

#include <QtGui/QInputDialog>
#include <QtGui/QMessageBox>

QString UIgetText(QWidget* parent, const QString& title, const QString& labletext, const QString& deftext, bool& ok)
{
    QInputDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setLabelText(labletext);
    dialog.setTextValue(deftext);
    dialog.setTextEchoMode(QLineEdit::Normal);

    ok = !!(dialog.exec());
    if (ok)
        return dialog.textValue();

    return QString();
}

QMessageBox::StandardButton YesNoCancel(QWidget *parent, const QString &title, const QString& text)
{
    QMessageBox msgBox(QMessageBox::Information, title, text, QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, parent);
    if (msgBox.exec() == -1)
        return QMessageBox::Cancel;

    return msgBox.standardButton(msgBox.clickedButton());
}

void UIOKMessage(QWidget* parent, const QString& title, const QString& text)
{
    QMessageBox msgBox(QMessageBox::Information, title, text, QMessageBox::Ok, parent);
    msgBox.exec();
}

QString UIDisplayName(const QString& fullname)
{
    int index = fullname.indexOf(QLatin1Char(':'));
    if (index < 0)
        return QString();

    return fullname.mid(index+1);
}
