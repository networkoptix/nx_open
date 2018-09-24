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
* QnLayoutFileStorageResource uses for layout export
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
    virtual ~QnLayoutFileStorageResource();

    static QnStorageResource* instance(QnCommonModule* commonModule, const QString&);

    // Whether the file is crypted or intended to be crypted.
    bool isCrypted() const;
    // Set new password if we intend to create a new layout file. Please call before `open`.
    void usePasswordForExport(const QString& password);
    // Set password for reading existing layout file.
    bool usePasswordToOpen(const QString& password);

    virtual void setUrl(const QString& value) override;

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

    void dumpStructure();

private:
    bool readIndexHeader();
    bool writeIndexHeader();

    int getTailSize() const;
    void writeFileTail(QFile& file);

    void closeOpenedFiles();
    void restoreOpenedFiles();

    bool shouldCrypt(const QString& streamName);

    static QString stripName(const QString& fileName); //< Returns part after '?'.

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
};

typedef QSharedPointer<QnLayoutFileStorageResource> QnLayoutFileStorageResourcePtr;

#endif // _LAYOUT_STORAGE_PROTOCOL_H__
