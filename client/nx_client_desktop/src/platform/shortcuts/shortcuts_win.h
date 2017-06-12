#ifndef QN_WINDOWS_SHORTCUTS_H
#define QN_WINDOWS_SHORTCUTS_H

#include "platform_shortcuts.h"

class QnWindowsShortcuts: public QnPlatformShortcuts {
    Q_OBJECT
public:
    QnWindowsShortcuts(QObject *parent = NULL);

    virtual bool createShortcut(const QString &sourceFile, const QString &destinationPath, const QString &name, const QStringList &arguments, int iconId = 0) override;
    virtual bool deleteShortcut(const QString &destinationPath, const QString &name) const override;
    virtual bool shortcutExists(const QString &destinationPath, const QString &name) const override;
    virtual bool supported() const override;
};


#endif // QN_WINDOWS_SHORTCUTS_H
