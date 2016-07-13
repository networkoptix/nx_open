#pragma once

/**
 * Class for directory backup and restore.
 * Does not supports recursive structures. Only plain files list.
 */
class QnDirectoryBackup
{
public:
    enum class Behavior
    {
        Copy,
        Move
    };

    QnDirectoryBackup(const QString& originalDirectory, const QString& backupDirectory);
    QnDirectoryBackup(const QString& originalDirectory, const QStringList& nameFilters, const QString& backupDirectory);

    /** Backup contents of the source directory into backup directory. */
    bool backup(Behavior behavior = Behavior::Copy) const;

    /** Restore contents from the backup directory. */
    bool restore(Behavior behavior = Behavior::Copy) const;

private:
    enum class OverwritePolicy
    {
        Skip,       /*< Files with same names will not be overridden. */
        CheckSize,  /*< Files with different size will be overridden.  */
        CheckDate,  /*< Older files will be overridden. */
        Forced      /*< All files will be overridden. */
    };

    QStringList calculateFilenames(const QString& originalDirectory, const QStringList& nameFilters) const;
    bool copyFiles(const QString& sourceDirectory, const QString& targetDirectory, OverwritePolicy overwritePolicy) const;
    bool deleteFiles(const QString& targetDirectory) const;

private:
    const QString m_originalDirectory;
    const QString m_backupDirectory;
    const QStringList m_filenames;
};
