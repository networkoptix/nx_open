#include "migrate_oldwin_dir.h"

#ifdef Q_OS_WIN32

#include <array>

#include <QtCore/QDir>

#include <nx/utils/log/log.h>

namespace nx {
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

class MigrateOldWindowsDataHelper
{
public:
    MigrateOldWindowsDataHelper(MigrateDataHandler* handler):
        m_handler(handler),
        m_currentDataDir(handler->currentDataDir().toLower()),
        m_windowsDir(handler->windowsDir().toLower())
    {
        NX_VERBOSE(this, lit("[Moving data] Current data dir: %1, windows dir: %2")
                .arg(m_currentDataDir)
                .arg(m_windowsDir));
    }

    MigrateDataResult moveData()
    {
        if (m_windowsDir.isEmpty())
            return MigrateDataResult::WinDirNotFound;

        if (!m_currentDataDir.startsWith(m_windowsDir))
            return MigrateDataResult::NoNeedToMigrate;

        populateOldDataDirsCandidates();
        for (const QString& candidate: m_oldDataDirCandidates)
        {
            auto result = tryToMoveFromCandidate(candidate);
            if (result == MigrateDataResult::MoveDataFailed || result == MigrateDataResult::Ok)
                return result;
        }

        return MigrateDataResult::NoNeedToMigrate;
    }

private:
    void populateOldDataDirsCandidates()
    {
        const QString basePath = m_windowsDir.mid(0, m_windowsDir.lastIndexOf(lit("\\")) + 1);
        const QString dataSubPath = m_currentDataDir.mid(m_windowsDir.size());

        QString baseName = lit("windows");
        std::array<QString, 2> suffixes = {lit(".old"), lit(".000")};

        for (const auto& suffix : suffixes)
        {
            m_oldDataDirCandidates.append(basePath + baseName + suffix + dataSubPath);
            m_oldDataDirCandidates.append(basePath + baseName + suffix + lit("\\") + baseName + dataSubPath);
        }
    }

    MigrateDataResult tryToMoveFromCandidate(const QString& oldDataDirCandidate)
    {
        if (!m_handler->dirExists(oldDataDirCandidate))
        {
            NX_VERBOSE(this, lit("[Moving data] candidate: %1 doesn't exist").arg(oldDataDirCandidate));
            return MigrateDataResult::NoNeedToMigrate;
        }

        if (!m_handler->makePath(m_currentDataDir) || !m_handler->moveDir(oldDataDirCandidate, m_currentDataDir))
            return MigrateDataResult::MoveDataFailed;

        m_handler->rmDir(oldDataDirCandidate);
        return MigrateDataResult::Ok;
    }

private:
    MigrateDataHandler* m_handler;
    QString m_currentDataDir;
    QString m_windowsDir;
    QStringList m_oldDataDirCandidates;
};

MigrateDataResult migrateFilesFromWindowsOldDir(MigrateDataHandler* handler)
{
    MigrateOldWindowsDataHelper helper(handler);
    return helper.moveData();
}

ServerDataMigrateHandler::ServerDataMigrateHandler(const QString& dataDir):
    m_dataDir(dataDir)
{
}

QString ServerDataMigrateHandler::currentDataDir() const
{
    return QDir::toNativeSeparators(m_dataDir);
}

QString ServerDataMigrateHandler::windowsDir() const
{
    WCHAR lpBuffer[MAX_PATH];
    if (GetWindowsDirectory(lpBuffer, sizeof(lpBuffer)) == 0)
        return QString(); //< nothing to migrate
    return QString::fromUtf16((const ushort*)lpBuffer);
}

bool ServerDataMigrateHandler::dirExists(const QString& path) const
{
    return QDir(path).exists();
}

bool ServerDataMigrateHandler::makePath(const QString& path)
{
    return QDir().mkpath(path);
}

bool ServerDataMigrateHandler::moveDir(const QString& source, const QString& target)
{
    return nx::misc::moveDir(QDir(source), QDir(target));
}

bool ServerDataMigrateHandler::rmDir(const QString& path)
{
    return QDir().rmdir(path);
}

} // namespace misc
} // namespace nx

#endif // Q_OS_WIN32
