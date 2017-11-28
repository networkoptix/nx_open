#include "stored_file_data_provider.h"

#include <api/app_server_connection.h>
#include <core/resource/local_audio_file_resource.h>
#include <core/resource/resource.h>

#include <nx_ec/ec_api.h>

#include <nx/utils/random.h>
#include <core/resource/avi/avi_archive_delegate.h>
#include <utils/common/sleep.h>

namespace {
    const QString kNotificationsPathPrefix("notifications/");
} // namespace

QnStoredFileDataProvider::QnStoredFileDataProvider(
    ec2::AbstractECConnectionPtr connection,
    const QString &filePath,
    int cyclesCount)
:
    QnAbstractStreamDataProvider(QnResourcePtr(new QnResource())),
    m_state(StoredFileDataProviderState::Waiting),
    m_filePath(kNotificationsPathPrefix + filePath),
    m_storage(new QnExtIODeviceStorageResource(connection->commonModule())),
    m_cyclesCount(cyclesCount)

{
    connect(
        this, &QnStoredFileDataProvider::fileLoaded,
        this, &QnStoredFileDataProvider::at_fileLoaded);

    connection->getStoredFileManager(Qn::kSystemAccess)->getStoredFile(
        m_filePath,
        this,
        [this](int handle, ec2::ErrorCode errorCode, const QByteArray& fileData)
        {
            Q_UNUSED(handle);
            emit fileLoaded(fileData, errorCode == ec2::ErrorCode::ok);
        });
}

void QnStoredFileDataProvider::setCyclesCount(int cyclesCount)
{
    m_cyclesCount = cyclesCount;
}

void QnStoredFileDataProvider::prepareIODevice()
{
    const QString temporaryFilePath = QString::number(nx::utils::random::number());

    QBuffer* buffer = new QBuffer();
    buffer->setBuffer(&m_fileData);
    buffer->open(QIODevice::ReadOnly);

    m_storage->registerResourceData(temporaryFilePath, buffer);
    m_resource->setUrl(temporaryFilePath);

}

void QnStoredFileDataProvider::at_fileLoaded(const QByteArray& fileData, bool status)
{
    m_fileData = fileData;
    m_resource = QnResourcePtr(new LocalAudioFileResource());

    prepareIODevice();

    QnAviArchiveDelegatePtr mediaFileReader(new QnAviArchiveDelegate());
    mediaFileReader->setStorage(m_storage);

    if (!mediaFileReader->open(m_resource, /*archiveIntegrityWatcher*/ nullptr))
        return;

    m_provider = mediaFileReader;
    m_provider->setAudioChannel( 0 );
    m_state = StoredFileDataProviderState::Ready;
    m_cond.wakeOne();

}

QnStoredFileDataProvider::~QnStoredFileDataProvider()
{
    stop();
}

void QnStoredFileDataProvider::run()
{
    QnMutexLocker lock(&m_mutex);
    auto cyclesDone = m_cyclesCount;
    while(m_state != StoredFileDataProviderState::Ready)
        m_cond.wait(lock.mutex());

    while(!m_needStop)
    {
        QnAbstractMediaDataPtr data;
        while(!m_needStop)
        {
            if(!dataCanBeAccepted())
            {
                QnSleep::msleep(5);
                continue;
            }

            data = getNextData();

            if(!data)
                break;

            putData(data);
        }

        if (m_cyclesCount == 0 || ++cyclesDone < m_cyclesCount)
        {
            prepareIODevice();
            m_provider->seek(0, false);
        }
        else
        {
            break;
        }
    }

    afterRun();
}

void QnStoredFileDataProvider::afterRun()
{
    QnAbstractMediaDataPtr endOfStream(new QnEmptyMediaData());
    putData(endOfStream);
}

QnAbstractMediaDataPtr QnStoredFileDataProvider::getNextData()
{
    if (!m_provider)
        return nullptr;

    auto data = m_provider->getNextData();
    if (data)
        data->dataProvider = this;
    return data;
}
