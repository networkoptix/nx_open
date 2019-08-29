#pragma once

#include "resource.h"
#include <nx/utils/url.h>

#include <QtCore/QtCore>
#include <memory>

class QnAbstractStorageResource: public QnResource
{
    using base_type = QnResource;

public:
    QnAbstractStorageResource(QnCommonModule* commonModule):
        base_type(commonModule)
    {
    }

    enum cap
    {
        ListFile        = 0x0001,                   // capable of listing files
        RemoveFile      = 0x0002,                   // capable of removing files
        ReadFile        = 0x0004,                   // capable of reading files
        WriteFile       = 0x0008,                   // capable of writing files
        DBReady         = 0x0010                    // capable of DB hosting
    };

    class FileInfo
    {
    public:
        explicit FileInfo(const QString& uri, qint64 size, bool isDir = false):
            m_fpath(QDir::toNativeSeparators(uri)), m_size(size), m_isDir(isDir)
        {
        }

        explicit FileInfo(const QFileInfo& qfi) : m_qFileInfo(new QFileInfo(qfi)) {}
        FileInfo() = default;

        bool operator==(const FileInfo& other) const
        {
            return absoluteFilePath() == other.absoluteFilePath();
        }

        bool operator !=(const FileInfo& other) const
        {
            return !operator==(other);
        }

        static FileInfo fromQFileInfo(const QFileInfo &fi)
        {
            return FileInfo(fi);
        }

        bool isDir() const
        {
            return m_qFileInfo ? m_qFileInfo->isDir() : m_isDir;
        }

        QString absoluteFilePath() const
        {
            return m_qFileInfo ? m_qFileInfo->absoluteFilePath() : m_fpath;
        }

        QString absoluteDirPath() const
        {
            if (m_qFileInfo)
                return m_qFileInfo->absoluteDir().path();

            if (m_isDir)
                return m_fpath;

            const auto lastSeparatorIndex = m_fpath.lastIndexOf(QDir().separator());
            if (lastSeparatorIndex == -1)
                return m_fpath;

            return m_fpath.left(lastSeparatorIndex);
        }

        QString fileName() const
        {
            if (m_qFileInfo)
                return m_qFileInfo->fileName();

            const auto lastSeparatorIndex = m_fpath.lastIndexOf(QDir().separator());
            if (lastSeparatorIndex == -1)
                return m_fpath;

            return m_fpath.mid(lastSeparatorIndex + 1);
        }

        QString baseName() const
        {
            if (m_qFileInfo)
                return m_qFileInfo->baseName();

            const auto fileName = this->fileName();
            const int dotIndex = fileName.indexOf('.');
            if (dotIndex == -1)
                return fileName;

            return fileName.left(dotIndex);
        }

        qint64 size() const
        {
            return m_qFileInfo ? m_qFileInfo->size() : m_size;
        }

        QDateTime created() const
        {
            return m_qFileInfo ? m_qFileInfo->created() : QDateTime();
        }

        QString extension() const
        {
            const auto fileName = this->fileName();
            const int lastDotIndex = fileName.lastIndexOf('.');
            if (lastDotIndex == -1)
                return "";

            return fileName.mid(lastDotIndex + 1);
        }

        QString toString() const
        {
            return nx::utils::url::hidePassword(absoluteFilePath());
        }

    private:
        QString m_fpath;
        qint64 m_size = 0;
        bool m_isDir = false;
        std::shared_ptr<QFileInfo> m_qFileInfo;
    };

    typedef QList<FileInfo> FileInfoList;

    static FileInfoList FIListFromQFIList(const QFileInfoList& l)
    {
        FileInfoList ret;
        for (const auto& fi: l)
            ret.append(FileInfo(fi));
        return ret;
    }

public:
    static const qint64 kUnknownSize = -1;                  /**< Size of the storage cannot be calculated. */
    static const qint64 kSizeDetectionOmitted = -2;         /**< Size calculating was skipped. */

    virtual QIODevice *open(const QString &fileName, QIODevice::OpenMode openMode) = 0;

    /**
    *   \return storage capabilities ('cap' flags)
    */
    virtual int getCapabilities() const = 0;

    /**
     * \returns                         Storage free space in bytes, or <tt>kUnknownSize</tt> if this function is not supported.
     */
    virtual qint64 getFreeSpace() = 0;

    /**
     * \returns                         Storage total space in bytes, or <tt>kUnknownSize</tt> if this function is not supported.
     */
    virtual qint64 getTotalSpace() const = 0;

    ///**
    // * \returns                         Whether the storage is physically accessible.
    // */
    virtual Qn::StorageInitResult initOrUpdate() = 0;

    /**
     * \param url                       Url of the file to delete.
     * \returns                         Whether the file was successfully removed.
     */
    virtual bool removeFile(const QString& url) = 0;

    /**
     * \param url                       Url of the folder to remove.
     * \returns                         Whether the folder was successfully removed.
     */
    virtual bool removeDir(const QString& url) = 0;

    /**
     * This function is used when server restarts. Unfinished files re-readed, writed again (under a new name), then renamed.
     *
     * \param oldName
     * \param newName
     * \returns                         Whether the file was successfully renamed.
     */
    virtual bool renameFile(const QString& oldName, const QString& newName) = 0;

    /**
     * \param dirName                   Url of the folder to list.
     * \returns                         List of files in given directory, excluding subdirectories.
     *
     * \note QFileInfo structure MUST contains valid fields for full file name and file size.
     */
    virtual FileInfoList getFileList(const QString& dirName) = 0;

    /**
     * \param url                       Url of a file to check.
     * \returns                         Whether a file with the given url exists.
     */
    virtual bool isFileExists(const QString& url) = 0;

    /**
     * \param url                       Url of a folder to check.
     * \returns                         Whether a folder with the given name exists.
     */
    virtual bool isDirExists(const QString& url) = 0;

    /**
     * \param url                       Url of the file to get size of.
     * \returns                         Size of the file, or 0 if the file does not exist.
     */
    virtual qint64 getFileSize(const QString& url) const = 0;
};
