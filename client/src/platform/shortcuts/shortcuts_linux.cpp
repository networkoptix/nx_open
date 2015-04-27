#include "shortcuts_linux.h"

QnLinuxShortcuts::QnLinuxShortcuts(QObject *parent /*= NULL*/):
    QnPlatformShortcuts(parent)
{

}

bool QnLinuxShortcuts::createShortcut(const QString &sourceFile, const QString &destinationPath, const QString &name, const QStringList &arguments, int iconId)
{
    /*
    QFile link(strDstPath + QDir::separator() + strName + ".desktop");
    if (link.open(QFile::WriteOnly | QFile::Truncate))
    {
    QTextStream out(&link);
    out.setCodec("UTF-8");
    out << "[Desktop Entry]" << endl
    << "Encoding=UTF-8" << endl
    << "Version=1.0" << endl
    << "Type=Link" << endl
    << "Name=" << strName << endl
    << "URL=" << strSrcFile << endl
    << "Icon=icon-name" << endl;
    return true;
    }
    */

    return false;
}

bool QnLinuxShortcuts::deleteShortcut(const QString &destinationPath, const QString &name) const {
    return true;
}

bool QnLinuxShortcuts::shortcutExists(const QString &destinationPath, const QString &name) const {
    return false;
}

bool QnLinuxShortcuts::supported() const {
    return false;
}
