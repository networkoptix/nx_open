#ifdef Q_OS_WIN32

#include "migrate_oldwin_dir.h"

namespace misc {

bool moveDir(const QDir& srcDir, const QDir& dstDir)
{
    auto entries = srcDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files);
    for (const auto& entry: entries)
    {
        const QString name = entry.fileName();
        if (entry.isDir())
        {
            if (!dstDir.exists(name) && !dstDir.mkdir(name))
                return false;
            QDir srcSubDir(srcDir.absoluteFilePath(name));
            QDir dstSubDir(dstDir.absoluteFilePath(name));
            if (!moveDir(srcSubDir, dstSubDir))
                return false;
            srcDir.rmdir(name); //< ignore old dir related error
        }
        else if (entry.isFile())
        {
            QString srcFileName = srcDir.absoluteFilePath(name);
            QString dstFileName = dstDir.absoluteFilePath(name);
            if (QFile::exists(dstFileName))
                QFile::remove(srcFileName); //< don't override dst files
            else if (!QFile::rename(srcFileName, dstFileName))
                return false;
        }
    }

    return true;
}

void migrateFilesFromWindowsOldDir(const QString& currentDataDir)
{
    WCHAR lpBuffer[MAX_PATH];
    if (GetWindowsDirectory(lpBuffer, sizeof(lpBuffer)) == 0)
        return; //< nothing to migrate
    QString windowsDir = QString::fromUtf16((const ushort*)lpBuffer);
    QString oldWindowsDir = windowsDir + QLatin1String(".old");

    if (!currentDataDir.startsWith(windowsDir))
        return; //< nothing to migrate

    QString suffix = currentDataDir.mid(windowsDir.length());
    QString oldDirName = oldWindowsDir + suffix;

    QDir oldDir(oldDirName);
    if (!oldDir.exists())
        return;
    QDir dstDir(currentDataDir);
    if (!dstDir.mkpath(currentDataDir) || !moveDir(oldDir, dstDir))
        qWarning() << "Found application data in" << oldWindowsDir << "folder but data moving is failed";
    oldDir.rmdir(oldDirName); //< ignore old dir related error
}

} // namespace misc

#endif // Q_OS_WIN32
