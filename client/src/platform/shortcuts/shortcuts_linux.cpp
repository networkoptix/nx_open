#include "shortcuts_linux.h"

QnLinuxShortcuts::QnLinuxShortcuts(QObject *parent /*= NULL*/):
    QnPlatformShortcuts(parent)
{

}

bool QnLinuxShortcuts::createShortcut(const QString &sourceFile, const QString &destinationPath, const QString &name, const QStringList &arguments)
{
    return false;
}

bool QnLinuxShortcuts::shortcutExists(const QString &destinationPath, const QString &name) const 
{
    return false;
}
