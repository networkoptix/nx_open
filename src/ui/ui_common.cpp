#include "ui_common.h"

#include "settings.h"
#include "device_settings\style.h"

QString UIgetText(QWidget* parent, const QString& title, const QString& labletext, const QString& deftext, bool& ok)
{
	QInputDialog dialog(parent);

	dialog.setWindowOpacity(global_dlg_opacity);
	QStyle *arthurStyle = new ArthurStyle();
	dialog.setStyle(arthurStyle);

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
	msgBox.setWindowOpacity(global_dlg_opacity);
	QStyle *arthurStyle = new ArthurStyle();
	msgBox.setStyle(arthurStyle);

	if (msgBox.exec() == -1)
		return QMessageBox::Cancel;

	return msgBox.standardButton(msgBox.clickedButton());
}

void UIOKMessage(QWidget* parent, const QString& title, const QString& text)
{
	QMessageBox msgBox(QMessageBox::Information, title, text, QMessageBox::Ok, parent);
	msgBox.setWindowOpacity(global_dlg_opacity);
	QStyle *arthurStyle = new ArthurStyle();
	msgBox.setStyle(arthurStyle);

	msgBox.exec();
}

QString UIDisplayName(const QString& fullname)
{
	int index = fullname.indexOf(":");
	if (index<0)
		return QString();

	return fullname.mid(index+1);
}