#ifndef QN_PLATFORM_SHORTCUTS_H
#define QN_PLATFORM_SHORTCUTS_H

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
        int iconId = 0;
    };

public:
    QnPlatformShortcuts(QObject *parent = NULL): QObject(parent) {}
    virtual ~QnPlatformShortcuts() {}

    /**
     * This function creates a shortcut for an executable file.
     *
     * \param sourceFile                Path to the source executable file.
     * \param destinationPath           Path where the shortcut should be created (without filename).
     * \param name                      Name of the shortcut.
     * \param arguments                 Arguments to run the source executable.
     * \param iconId                    Id of the custom icon.
     * \returns                         True if shortcut creation was successful, false otherwise.
     */
    virtual bool createShortcut(const QString &sourceFile, const QString &destinationPath, const QString &name, const QStringList &arguments, int iconId = 0) = 0;

    /**
     * This function deletes a target shortcut if it exists.
     *
     * \param destinationPath           Path to the shortcut (without filename).
     * \param name                      Name of the shortcut.
     * \returns                         True if the shortcut does not exists anymore.
     */
    virtual bool deleteShortcut(const QString &destinationPath, const QString &name) const = 0;

    /**
     * This function checks if a target shortcut already exists.
     *
     * \param destinationPath           Path to the shortcut (without filename).
     * \param name                      Name of the shortcut.
     * \returns                         True if the shortcut exists, false otherwise.
     */
    virtual bool shortcutExists(const QString &destinationPath, const QString &name) const = 0;

    /**
     * This function collects some information about existing shortcut and returns it as a struct.
     *
     * @param destinationPath Path to the shortcut (without filename).
     * @param name Name of the shortcut.
     * @return Struct with parameters of the shortcut.
     */
    virtual ShortcutInfo getShortcutInfo(const QString& destinationPath, const QString& name) const = 0;

    virtual bool supported() const = 0;

private:
    Q_DISABLE_COPY(QnPlatformShortcuts)
};


#endif // QN_PLATFORM_SHORTCUTS_H
