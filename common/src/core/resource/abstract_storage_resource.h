#ifndef __ABSTRACT_STORAGE_H__
#define __ABSTRACT_STORAGE_H__

#include "resource.h"
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
        typedef std::shared_ptr<QFileInfo> QFileInfoPtr;

    public: // ctors
        explicit
        FileInfo(const QString& uri, qint64 size, bool isDir = false)
            : m_fpath(uri),
              m_size(size),
              m_isDir(isDir)
        {
            parseUri();
        }

        explicit
        FileInfo(const QFileInfo& qfi)
            : m_qfi(new QFileInfo(qfi))
        {}

    public:
        static FileInfo fromQFileInfo(const QFileInfo &fi)
        {
            return FileInfo(fi);
        }

        bool isDir() const
        {
            return m_qfi ? m_qfi->isDir() : m_isDir;
        }

        QString absoluteFilePath() const
        {
            return m_qfi ? m_qfi->absoluteFilePath() : m_fpath;
        }

        QString fileName() const
        {
            return m_qfi ? m_qfi->fileName() : m_fname;
        }

        QString baseName() const
        {
            return m_qfi ? m_qfi->baseName() : m_basename;
        }

        qint64 size() const
        {
            return m_qfi ? m_qfi->size() : m_size;
        }

        QDateTime created() const
        {
            return m_qfi ? m_qfi->created() : m_created;
        }

    private:
        void parseUri()
        {
            QStringList pathEntries = m_fpath.split(lit("/"));
            bool found = false;

            auto assignBN = [this]()
            {
                int dotIndex = m_fname.lastIndexOf(lit("."));
                if (dotIndex != -1)
                    m_basename = m_fname.left(dotIndex);
                else
                    m_basename = m_fname;
            };

            for (int i = pathEntries.size() - 1; i >= 0; --i)
            {
                if (!pathEntries[i].isEmpty())
                {
                    found = true;
                    m_fname = pathEntries[i];
                    assignBN();
                    break;
                }
            }

            if (!found && !m_fpath.isEmpty())
            {
                m_fname = m_fpath;
                assignBN();
            }
        }

    private:
        QString         m_fpath;
        QString         m_basename;
        QString         m_fname;
        qint64          m_size;
        bool            m_isDir;
        QFileInfoPtr    m_qfi;
        QDateTime       m_created;
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
    *   \return storage capabilities ('cap' flag(s))
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

#endif // __ABSTRACT_STORAGE_H__
