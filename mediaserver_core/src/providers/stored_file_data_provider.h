#pragma once
#include <core/dataprovider/abstract_streamdataprovider.h>
#include <plugins/storage/memory/ext_iodevice_storage.h>
#include <plugins/resource/avi/avi_archive_delegate.h>
#include <QtCore/QBuffer>


class QnStoredFileDataProvider : public QnAbstractStreamDataProvider
{

    enum class StoredFileDataProviderState
    {
        Waiting,
        Ready
    };

Q_OBJECT
public:
    explicit QnStoredFileDataProvider(const QString& filePath, int cyclesCount = 0);
    ~QnStoredFileDataProvider();

    virtual void run() override;

    virtual QnAbstractMediaDataPtr getNextData();

    void setCyclesCount(int cyclesCount);

private:
    void afterRun();
    void prepareIODevice();

signals:
    void fileLoaded(const QByteArray& fileData, bool ok);

protected slots:
    void at_fileLoaded(const QByteArray& fileData, bool ok);

private:
    QnResourcePtr m_resource;
    StoredFileDataProviderState m_state;
    mutable QnMutex m_mutex;
    mutable QnWaitCondition m_cond;
    QString m_filePath;
    QByteArray m_fileData;
    QnExtIODeviceStorageResourcePtr m_storage;
    QnAviArchiveDelegatePtr m_provider;
    int m_cyclesCount;
};


