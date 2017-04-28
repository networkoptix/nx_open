#include "directory_backup.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>

#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>

namespace {

enum class OverwritePolicy
{
    Skip,       /*< Files with same names will not be overridden. */
    CheckSize,  /*< Files with different size will be overridden.  */
    CheckDate,  /*< Older files will be overridden. */
    Forced      /*< All files will be overridden. */
};

bool copyFiles(const QString& sourceDirectory, const QString& targetDirectory,
    const QStringList& fileNames, OverwritePolicy overwritePolicy)
{

    /* Check if target file must be overridden by source file. */
    auto needOverwrite = [overwritePolicy](const QString& source, const QString& target)
    {
        switch (overwritePolicy)
        {
            case OverwritePolicy::Skip:
                return false;

            case OverwritePolicy::CheckDate:
                return QFileInfo(source).lastModified() >= QFileInfo(target).lastModified();

            case OverwritePolicy::CheckSize:
                return QFileInfo(source).size() != QFileInfo(target).size();

            case OverwritePolicy::Forced:
                return true;

            default:
                NX_ASSERT(false, Q_FUNC_INFO, "All cases must be handled.");
                break;
        }
        return false;
    };


    /* Always try to copy as much files as possible even if we have failed some. */
    bool success = true;
    for (const QString& filename : fileNames)
    {
        const QString sourceFilename = sourceDirectory + L'/' + filename;
        const QString targetFilename = targetDirectory + L'/' + filename;

        if (QFile::exists(targetFilename))
        {
            if (!needOverwrite(sourceFilename, targetFilename))
                continue;

            if (!QFile::remove(targetFilename))
            {
                NX_LOG(lit("Could not overwrite file %1")
                    .arg(targetFilename), cl_logERROR);
                success = false;
                continue;
            }
        }

        if (!QFile(sourceFilename).copy(targetFilename))
        {
            NX_LOG(lit("Could not copy file %1 to %2")
                .arg(sourceFilename)
                .arg(targetFilename), cl_logERROR);
            success = false;
        }
    }
    return success;
}

template<typename BackupsList>
bool rollback(QnDirectoryBackupBehavior behavior
    , const BackupsList& backups)
{
    for (auto it = backups.rbegin(); it != backups.rend(); ++it)
    {
        const auto backup = *it;
        if (!backup->backup(behavior))
            return false;
    }
    return true;
}

template<typename RestoreType>
bool restoreImpl(const QString& original, const QString& backup,
    const QStringList& filters, QnDirectoryBackupBehavior behavior)
{
    const auto restoreInstance = RestoreType(backup, filters, original);
    return restoreInstance.backup(behavior);
}

typedef QSharedPointer<QnDirectoryBackup> SingleDirectoryBackupPtr;
typedef QList<SingleDirectoryBackupPtr> BackupsList;

template<typename BackupsStack>
bool recursiveBackupImpl(const QString& sourceDirectory, const QString& targetDirectory,
    const QStringList& filters, QnDirectoryBackupBehavior behavior, BackupsStack& successfulBackups)
{
    const auto source = QDir(sourceDirectory);
    const auto target = QDir(targetDirectory);

    const auto entries = source.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    for (const auto& entry: entries)
    {
        const auto sourcePath = source.absoluteFilePath(entry);
        const auto targetPath = target.absoluteFilePath(entry);
        if (!recursiveBackupImpl(sourcePath, targetPath, filters, behavior, successfulBackups))
            return false;
    }

    // Bakups files in current firectory
    const auto filesBackup = SingleDirectoryBackupPtr(
        new QnDirectoryBackup(sourceDirectory, filters, targetDirectory));
    if (!filesBackup->backup(behavior))
        return false;

    successfulBackups.append(filesBackup);

    return true;
}

bool deleteFiles(const QString& targetDirectory, const QStringList& fileNames)
{
    /* Always try to delete as much files as possible even if we have failed some. */
    bool success = true;
    for (const QString& filename : fileNames)
    {
        const QString filePath = targetDirectory + L'/' + filename;
        if (!QFileInfo::exists(filePath))
            continue;

        if (!QFile::remove(filePath))
        {
            NX_LOG(lit("Could not delete file %1").arg(filePath), cl_logERROR);
            success = false;
        }
    }
    return success;
}

bool makeFilesBackup(QnDirectoryBackupBehavior behavior, const QString& from, const QString& to,
    const QStringList& fileNames)
{
    if (!QDir().mkpath(to))
        return false;

    if (!deleteFiles(to, fileNames))
    {
        NX_LOG(lit("Could not cleanup backup directory %1.").arg(to), cl_logERROR);
        return false;
    }

    bool copyResult = copyFiles(from, to,
        fileNames, OverwritePolicy::Forced);
    if (behavior == QnDirectoryBackupBehavior::Copy)
        return copyResult;

    NX_ASSERT(behavior == QnDirectoryBackupBehavior::Move);

    /* Copy files first and only then delete originals. */
    if (!copyResult)
        return false;

    return deleteFiles(from, fileNames);
}

QStringList calculateFilenames(const QString& directory, const QStringList& nameFilters)
{
    return QDir(directory).entryList(nameFilters, QDir::NoDotAndDotDot | QDir::Files);
}

} // Unnamed namespace

//////////////////////////////////////////////////////////////////////////////////////////////////

QnBaseDirectoryBackup::QnBaseDirectoryBackup(const QString& originalDirectory, const QStringList& nameFilters,
    const QString& backupDirectory)
    :
    m_originalDirectory(originalDirectory),
    m_backupDirectory(backupDirectory),
    m_filters(nameFilters)
{
}

const QString& QnBaseDirectoryBackup::originalDirectory() const
{
    return m_originalDirectory;
}

const QString& QnBaseDirectoryBackup::backupDirectory() const
{
    return m_backupDirectory;
}
const QStringList& QnBaseDirectoryBackup::filters() const
{
    return m_filters;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

QnDirectoryBackup::QnDirectoryBackup(const QString& originalDirectory, const QString& backupDirectory)
    :
    QnBaseDirectoryBackup(originalDirectory, QStringList(), backupDirectory),
    m_fileNames(calculateFilenames(originalDirectory, filters()))
{}

QnDirectoryBackup::QnDirectoryBackup(const QString& originalDirectory, const QStringList& nameFilters, const QString& backupDirectory)
    :
    QnBaseDirectoryBackup(originalDirectory, nameFilters, backupDirectory),
    m_fileNames(calculateFilenames(originalDirectory, filters()))
{}

bool QnDirectoryBackup::backup(QnDirectoryBackupBehavior behavior) const
{
    if (!makeFilesBackup(behavior, originalDirectory(), backupDirectory(), m_fileNames))
    {
        /* If deleting was not successful, try restore backup. */
        NX_LOG(lit("Could not cleanup original directory %1, trying to restore backup.")
            .arg(originalDirectory()), cl_logERROR);

        if (!copyFiles(backupDirectory(), originalDirectory(), m_fileNames, OverwritePolicy::Skip))
        {
            NX_LOG(lit("Could not restore backup. System is is invalid state."), cl_logERROR);
        }
        return false;
    }

    return true;
}

bool QnDirectoryBackup::restore(QnDirectoryBackupBehavior behavior) const
{
    return restoreImpl<QnDirectoryBackup>(originalDirectory(),
        backupDirectory(), filters(), behavior);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

QnDirectoryRecursiveBackup::QnDirectoryRecursiveBackup(const QString& originalDirectory,
    const QString& backupDirectory)
    :
    QnBaseDirectoryBackup(originalDirectory, QStringList(), backupDirectory)
{
}

QnDirectoryRecursiveBackup::QnDirectoryRecursiveBackup(const QString& originalDirectory,
    const QStringList& nameFilters, const QString& backupDirectory)
    :
    QnBaseDirectoryBackup(originalDirectory, nameFilters, backupDirectory)
{
}

bool QnDirectoryRecursiveBackup::backup(QnDirectoryBackupBehavior behavior) const
{
    BackupsList backups;
    if (recursiveBackupImpl(originalDirectory(), backupDirectory(), filters(), behavior, backups))
        return true;

    rollback(behavior, backups);
    return false;
}

bool QnDirectoryRecursiveBackup::restore(QnDirectoryBackupBehavior behavior) const
{
    return restoreImpl<QnDirectoryRecursiveBackup>(originalDirectory(),
        backupDirectory(), filters(), behavior);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

QnMultipleDirectoriesBackup::QnMultipleDirectoriesBackup()
    :
    QnBaseDirectoryBackup(QString(), QStringList(), QString()),
    m_backups()
{
}

void QnMultipleDirectoriesBackup::addDirectoryBackup(const SingleDirectoryBackupPtr& backup)
{
    m_backups.append(backup);
}

bool QnMultipleDirectoriesBackup::backup(QnDirectoryBackupBehavior behavior) const
{
    BackupsList successfulBackups;
    for (const auto backup : m_backups)
    {
        if (backup->backup(behavior))
        {
            successfulBackups.append(backup);
            continue;
        }

        rollback(behavior, successfulBackups);
        return false;
    }
    return true;
}

bool QnMultipleDirectoriesBackup::restore(QnDirectoryBackupBehavior behavior) const
{
    return rollback(behavior, m_backups);
}
