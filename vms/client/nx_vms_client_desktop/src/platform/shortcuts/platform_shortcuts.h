// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_PLATFORM_SHORTCUTS_H
#define QN_PLATFORM_SHORTCUTS_H

#include <QVariant>

#include <QtCore/QObject>

/**
 * The <tt>QnPlatformShortcuts</tt> class is intended for access to platform's shortcuts.
 */
class QnPlatformShortcuts: public QObject {
    Q_OBJECT

public:
    struct ShortcutInfo
    {
        QString sourceFile;
        QStringList arguments;
        QString iconPath;
        QVariant icon;
    };

public:
    QnPlatformShortcuts(QObject *parent = nullptr): QObject(parent) {}
    virtual ~QnPlatformShortcuts() {}

    /**
     * This function creates a shortcut for an executable file.
     *
     * \param sourceFile              Path to the source executable file.
     * \param destinationDir          Path where the shortcut should be created (without filename).
     * \param name                    Name of the shortcut.
     * \param arguments               Arguments to run the source executable.
     * \param icon                    Custom icon (id number on Windows, name/path otherwise).
     * \returns                       True if shortcut creation was successful, false otherwise.
     */
    virtual bool createShortcut(
        const QString& sourceFile,
        const QString& destinationDir,
        const QString& name,
        const QStringList& arguments = {},
        const QVariant& icon = QVariant()) = 0;

    /**
     * This function deletes a target shortcut if it exists.
     *
     * \param destinationDir            Path to the shortcut (without filename).
     * \param name                      Name of the shortcut.
     * \returns                         True if the shortcut does not exists anymore.
     */
    virtual bool deleteShortcut(const QString& destinationDir, const QString& name) const = 0;

    /**
     * This function checks if a target shortcut already exists.
     *
     * \param destinationDir            Path to the shortcut (without filename).
     * \param name                      Name of the shortcut.
     * \returns                         True if the shortcut exists, false otherwise.
     */
    virtual bool shortcutExists(const QString& destinationDir, const QString& name) const = 0;

    /**
     * This function collects some information about existing shortcut and returns it as a struct.
     *
     * @param destinationDir Path to the shortcut (without filename).
     * @param name Name of the shortcut.
     * @return Struct with parameters of the shortcut.
     */
    virtual ShortcutInfo getShortcutInfo(const QString& destinationDir, const QString& name) const = 0;

    virtual bool supported() const = 0;

private:
    Q_DISABLE_COPY(QnPlatformShortcuts)
};


#endif // QN_PLATFORM_SHORTCUTS_H
