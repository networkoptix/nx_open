#ifndef QN_PLATFORM_SHORTCUTS_H
#define QN_PLATFORM_SHORTCUTS_H

#include <QtCore/QObject>

/**
 * The <tt>QnPlatformShortcuts</tt> class is intended for access to platform's shortcuts.
 */
class QnPlatformShortcuts: public QObject {
    Q_OBJECT
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
     * \returns                         True if shortcut creation was successful, false otherwise.
     */
    virtual bool createShortcut(const QString &sourceFile, const QString &destinationPath, const QString &name, const QStringList &arguments) = 0;

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

    virtual bool supported() const = 0;

private:
    Q_DISABLE_COPY(QnPlatformShortcuts)
};


#endif // QN_PLATFORM_SHORTCUTS_H
