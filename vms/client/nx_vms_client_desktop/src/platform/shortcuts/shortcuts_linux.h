#ifndef QN_LINUX_SHORTCUTS_H
#define QN_LINUX_SHORTCUTS_H

#include "platform_shortcuts.h"

class QnLinuxShortcuts: public QnPlatformShortcuts {
    Q_OBJECT
public:
    QnLinuxShortcuts(QObject *parent = NULL);

    virtual bool createShortcut(const QString &sourceFile, const QString &destinationPath, const QString &name, const QStringList &arguments, int iconId = 0) override;
    virtual bool deleteShortcut(const QString &destinationPath, const QString &name) const override;
    virtual bool shortcutExists(const QString &destinationPath, const QString &name) const override;
    virtual ShortcutInfo getShortcutInfo(const QString& destinationPath, const QString& name) const override;
    virtual bool supported() const override;
};


#endif // QN_LINUX_SHORTCUTS_H
