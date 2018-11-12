#pragma once

#include <QtCore/QString>
#include <QtCore/QFile>

#include <nx/utils/thread/mutex.h>

#include "layout_storage_stream.h"
#include "layout_storage_resource.h"

// This layout storage stream is used when no encryption is involved.
class QnLayoutPlainStream:
    public QIODevice,
    public QnLayoutStreamSupport
{
public:
    QnLayoutPlainStream(QnLayoutFileStorageResource& storageResource, const QString& fileName);

    virtual ~QnLayoutPlainStream();

    virtual bool seek(qint64 offset) override;
    virtual qint64 pos() const override;
    virtual qint64 size() const override;

    virtual qint64 readData(char* data, qint64 maxSize) override;
    virtual qint64 writeData(const char* data, qint64 maxSize) override;

    virtual bool open(QIODevice::OpenMode openMode) override;
    virtual void close() override;

    virtual void storeStateAndClose() override;
    virtual void restoreState() override;

private:
    QFile m_file;
    mutable QnMutex m_mutex;
    QnLayoutFileStorageResource& m_storageResource;

    qint64 m_fileOffset;
    qint64 m_fileSize;
    QString m_fileName;
    qint64 m_storedPosition;
    QIODevice::OpenMode m_openMode;
};
