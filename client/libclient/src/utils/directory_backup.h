#pragma once


enum class QnDirectoryBackupBehavior
{
    Copy,
    Move
};

/**
* Base class for directory backup and restore.
*/
class QnBaseDirectoryBackup
{
public:
    virtual ~QnBaseDirectoryBackup() = default;

    /** Backup contents of the source directory into backup directory. */
    virtual bool backup(QnDirectoryBackupBehavior behavior) const = 0;

    /** Restore contents from the backup directory. */
    virtual bool restore(QnDirectoryBackupBehavior behavior) const = 0;

protected:
    QnBaseDirectoryBackup(const QString& originalDirectory, const QStringList& nameFilters, const QString& backupDirectory);

    const QString& originalDirectory() const;
    const QString& backupDirectory() const;
    const QStringList& filters() const;

private:
    const QString m_originalDirectory;
    const QString m_backupDirectory;
    const QStringList m_filters;
};

/**
* Class for directory backup and restore.
* Does not supports recursive structures. Only plain files list.
*/
class QnDirectoryBackup : public QnBaseDirectoryBackup
{
public:

    QnDirectoryBackup(const QString& originalDirectory, const QString& backupDirectory);
    QnDirectoryBackup(const QString& originalDirectory, const QStringList& nameFilters, const QString& backupDirectory);

public: //< Overrides
    bool backup(QnDirectoryBackupBehavior behavior) const override;
    bool restore(QnDirectoryBackupBehavior behavior) const override;

private:
    const QStringList m_fileNames;
};

/**
* Class for directory backup and restore.
* Supports recursive structures.
*/
class QnDirectoryRecursiveBackup : public QnBaseDirectoryBackup
{
public:
    QnDirectoryRecursiveBackup(const QString& originalDirectory, const QString& backupDirectory);

    QnDirectoryRecursiveBackup(const QString& originalDirectory, const QStringList& nameFilters, const QString& backupDirectory);

public: //< Overrides
    bool backup(QnDirectoryBackupBehavior behavior) const override;
    bool restore(QnDirectoryBackupBehavior behavior) const override;
};

/**
* Class for multiple directories backup and restore.
*/
class QnMultipleDirectoriesBackup: public QnBaseDirectoryBackup
{
public:
    QnMultipleDirectoriesBackup();

    typedef QSharedPointer<QnBaseDirectoryBackup> SingleDirectoryBackupPtr;
    void addDirectoryBackup(const SingleDirectoryBackupPtr& backup);

public: //< Overrides
    bool backup(QnDirectoryBackupBehavior behavior) const override;
    bool restore(QnDirectoryBackupBehavior behavior) const override;

private:
    typedef QList<SingleDirectoryBackupPtr> BackupsList;
    BackupsList m_backups;
};
