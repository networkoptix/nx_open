#ifndef _LAYOUT_STORAGE_PROTOCOL_H__
#define _LAYOUT_STORAGE_PROTOCOL_H__

#include <QtCore/QFile>

#include <core/resource/storage_resource.h>
#include <nx/core/layout/layout_file_info.h>

#include "layout_storage_stream.h"

extern "C"
{
    #include <libavformat/avio.h>
}

class QnLayoutPlainStream;
class QnTimePeriodList;

/*
* QnLayoutFileStorageResource is used for layout reading and export.
*/
class QnLayoutFileStorageResource: public QnStorageResource
{
    using base_type = QnStorageResource;
    using StreamIndexEntry = nx::core::layout::StreamIndexEntry;
    using StreamIndex = nx::core::layout::StreamIndex;
    using CryptoInfo = nx::core::layout::CryptoInfo;

public:
    enum StorageFlags {
        ReadOnly        = 0x1,
        ContainsCameras = 0x2,
    };

    QnLayoutFileStorageResource(QnCommonModule* commonModule);
    QnLayoutFileStorageResource(QnCommonModule* commonModule, const QString& url);

    virtual ~QnLayoutFileStorageResource();

    static QnStorageResource* instance(QnCommonModule* commonModule, const QString& url);

    /** Returns true if the layout file is actually encrypted. */
    virtual bool isEncrypted() const override;
    /** Returns true if the layout file is encrypted and no valid password is provided. */
    using Encryptable::requiresPassword;
    /** Attempts to set a password for opening existing layout file. */
    virtual bool usePasswordToRead(const QString& password) override;
    /** Sets a password for writing new layout. */
    virtual void setPasswordToWrite(const QString& password) override;
    /** Drops password. */
    virtual void forgetPassword() override;
    /** Returns password */
    virtual QString password() const override;

    virtual void setUrl(const QString& value) override; // URL for Layout File is always plain file path!

    virtual QIODevice* open(const QString& url, QIODevice::OpenMode openMode) override;

    virtual int getCapabilities() const override;
    virtual Qn::StorageInitResult initOrUpdate() override;
    virtual QnAbstractStorageResource::FileInfoList getFileList(const QString& dirName) override;
    qint64 getFileSize(const QString& url) const override;
    virtual bool removeFile(const QString& url) override;
    virtual bool removeDir(const QString& url) override;
    virtual bool renameFile(const QString& oldName, const QString& newName) override;
    virtual bool isFileExists(const QString& url) override;
    virtual bool isDirExists(const QString& url) override;
    virtual qint64 getFreeSpace() override;
    virtual qint64 getTotalSpace() const override;

    bool switchToFile(const QString& oldName, const QString& newName, bool dataInOldFile);

    QnTimePeriodList getTimePeriods(const QnResourcePtr &resource);

    static QString itemUniqueId(const QString& layoutUrl, const QString& itemUniqueId);

    virtual QString getPath() const override;

    // These functions interface with internal streams.
    struct Stream
    {
        qint64 position = 0;
        qint64 size = 0;

        operator bool () const { return position > 0; }
    };
    // Searches for a stream with the given name.
    Stream findStream(const QString& name);
    // Opens or adds new stream to the layout storage.
    Stream findOrAddStream(const QString& name);
    // Called from a output stream when it is closed.
    void finalizeWrittenStream(qint64 pos);
    void registerFile(QnLayoutStreamSupport* file);
    void unregisterFile(QnLayoutStreamSupport* file);
    // This functions also removes file from rename cache.
    void removeFileCompletely(QnLayoutStreamSupport* file);

    void dumpStructure();

    QnMutex& streamMutex(); // This is used to prevent deadlock when using stream objects.

private:
    bool readIndexHeader();
    bool writeIndexHeader();

    int getTailSize() const;
    void writeFileTail(QFile& file);

    void closeOpenedFiles();
    void restoreOpenedFiles();

    bool shouldCrypt(const QString& streamName);

    static QString stripName(const QString& fileName); //< Returns part after '?'.
    static QString getFileName(const QString& url); //< Strips protocol and parameters after '?'.

    void lockOpenings(); //< Used when renaming or deleting to disable extra opens.
    void unlockOpenings();

private:
    StreamIndex m_index;
    CryptoInfo m_cryptoInfo;
    QString m_password;

    QSet<QnLayoutStreamSupport*> m_openedFiles;
    QSet<QnLayoutStreamSupport*> m_cachedOpenedFiles;
    QnMutex m_fileSync;
    static QnMutex m_storageSync;
    static QSet<QnLayoutFileStorageResource*> m_allStorages;

    nx::core::layout::FileInfo m_info;
    bool m_lockedOpenings = false; //< Used to prevent stream openings when moving file.
};

typedef QSharedPointer<QnLayoutFileStorageResource> QnLayoutFileStorageResourcePtr;

#endif // _LAYOUT_STORAGE_PROTOCOL_H__
