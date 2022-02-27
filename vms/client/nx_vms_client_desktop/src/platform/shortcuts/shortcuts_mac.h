// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_MAC_SHORTCUTS_H
#define QN_MAC_SHORTCUTS_H

#include "platform_shortcuts.h"

class QnMacShortcuts: public QnPlatformShortcuts {
    Q_OBJECT
public:
    QnMacShortcuts(QObject *parent = nullptr);

    virtual bool createShortcut(const QString& sourceFilePath, const QString& destinationPath,
        const QString& name, const QStringList& arguments,
        const QVariant& icon = QVariant()) override;
    virtual bool deleteShortcut(const QString& destinationPath, const QString& name) const override;
    virtual bool shortcutExists(const QString& destinationPath, const QString& name) const override;
    virtual ShortcutInfo getShortcutInfo(const QString& destinationPath,
        const QString& name) const override;
    virtual bool supported() const override;
};


#endif // QN_MAC_SHORTCUTS_H
