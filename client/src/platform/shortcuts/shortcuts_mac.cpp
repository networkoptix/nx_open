#include "shortcuts_mac.h"

QnMacShortcuts::QnMacShortcuts(QObject *parent /*= NULL*/):
    QnPlatformShortcuts(parent)
{

}

bool QnMacShortcuts::createShortcut(const QString &sourceFile, const QString &destinationPath, const QString &name, const QStringList &arguments)
{
    return false;
}

bool QnMacShortcuts::shortcutExists(const QString &destinationPath, const QString &name) const 
{
    return false;
}
