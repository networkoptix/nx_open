#include "ui_common.h"

#include <QtGui/QInputDialog>
#include <QtGui/QMessageBox>
#include <QtGui/QPixmapCache>

#include "base/log.h"
#include "settings.h"

QPixmap cached(const QString &img)
{
    static bool first = true;
    if (first)
    {
        QPixmapCache::setCacheLimit(16 * 1024); // 16 MB
        first = false;
    }

    if (QPixmap *p = QPixmapCache::find(img))
        return *p;

    QPixmap pm = QPixmap::fromImage(QImage(img), Qt::OrderedDither | Qt::OrderedAlphaDither);
    if (pm.isNull())
    {
        cl_log.log(QLatin1String("Cannot load image "), img, cl_logERROR);
        QMessageBox msgBox;
        msgBox.setText(QMessageBox::tr("Error"));
        msgBox.setInformativeText(QMessageBox::tr("Cannot load image `%1`").arg(img));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.exec();

        return QPixmap();
    }

    QPixmapCache::insert(img, pm);
    return pm;
}

QString UIgetText(QWidget* parent, const QString& title, const QString& labletext, const QString& deftext, bool& ok)
{
	QInputDialog dialog(parent);
	//dialog.setWindowOpacity(global_dlg_opacity);
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
	//msgBox.setWindowOpacity(global_dlg_opacity);

	if (msgBox.exec() == -1)
		return QMessageBox::Cancel;

	return msgBox.standardButton(msgBox.clickedButton());
}

void UIOKMessage(QWidget* parent, const QString& title, const QString& text)
{
	QMessageBox msgBox(QMessageBox::Information, title, text, QMessageBox::Ok, parent);
	//msgBox.setWindowOpacity(global_dlg_opacity);

	msgBox.exec();
}

QString UIDisplayName(const QString& fullname)
{
	int index = fullname.indexOf(QLatin1Char(':'));
	if (index<0)
		return QString();

	return fullname.mid(index+1);
}
