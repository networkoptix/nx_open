#include "directory_backup.h"

#include <QtCore/QDir>

#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>

QnDirectoryBackup::QnDirectoryBackup(const QString& originalDirectory, const QString& backupDirectory) :
    QnDirectoryBackup(originalDirectory, QStringList(), backupDirectory)
{}

QnDirectoryBackup::QnDirectoryBackup(const QString& originalDirectory, const QStringList& nameFilters, const QString& backupDirectory) :
    m_originalDirectory(originalDirectory),
    m_backupDirectory(backupDirectory),
    m_filenames(calculateFilenames(originalDirectory, nameFilters))
{}

bool QnDirectoryBackup::backup(Behavior behavior) const
{
    if (!QDir().mkpath(m_backupDirectory))
        return false;

    if (!deleteFiles(m_backupDirectory))
    {
        NX_LOG(lit("Could not cleanup backup directory %1.").arg(m_backupDirectory), cl_logERROR);
        return false;
    }

    bool copyResult = copyFiles(m_originalDirectory, m_backupDirectory);
    if (behavior == Behavior::Copy)
        return copyResult;

    NX_ASSERT(behavior == Behavior::Move);

    /* Copy files first and only then delete originals. */
    if (!copyResult)
        return false;

    if (!deleteFiles(m_originalDirectory))
    {
        /* If deleting was not successful, try restore backup. */
        NX_LOG(lit("Could not cleanup original directory %1, trying to restore backup.").arg(m_originalDirectory), cl_logERROR);
        if (!copyFiles(m_backupDirectory, m_originalDirectory))
        {
            NX_LOG(lit("Could not restore backup. System is is invalid state."), cl_logERROR);
        }
        return false;
    }

    return true;
}

bool QnDirectoryBackup::restore(Behavior behavior) const
{
    if (!QDir().mkpath(m_originalDirectory))
        return false;

    if (!deleteFiles(m_originalDirectory))
    {
        NX_LOG(lit("Could not cleanup original directory %1.").arg(m_originalDirectory), cl_logERROR);
        return false;
    }

    bool copyResult = copyFiles(m_backupDirectory, m_originalDirectory);
    if (behavior == Behavior::Copy)
        return copyResult;

    NX_ASSERT(behavior == Behavior::Move);

    /* Copy files first and only then delete originals. */
    if (!copyResult)
        return false;

    /* Do nothing if cannot clear backup folder correctly. */
    return deleteFiles(m_backupDirectory);
}

QStringList QnDirectoryBackup::calculateFilenames(const QString& originalDirectory, const QStringList& nameFilters) const
{
    return QDir(originalDirectory).entryList(nameFilters, QDir::NoDotAndDotDot | QDir::Files);
}

bool QnDirectoryBackup::copyFiles(const QString& sourceDirectory, const QString& targetDirectory) const
{
    /* Always try to copy as much files as possible even if we have failed some. */
    bool success = true;
    for (const QString& filename : m_filenames)
    {
        const QString sourceFilename = sourceDirectory + L'/' + filename;
        const QString targetFilename = targetDirectory + L'/' + filename;
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

bool QnDirectoryBackup::deleteFiles(const QString& targetDirectory) const
{
    /* Always try to delete as much files as possible even if we have failed some. */
    bool success = true;
    for (const QString& filename : m_filenames)
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
