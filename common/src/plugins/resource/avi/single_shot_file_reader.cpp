#include "single_shot_file_reader.h"

#ifdef ENABLE_ARCHIVE

#include "filetypesupport.h"
#include "utils/common/synctime.h"
#include "core/datapacket/video_data_packet.h"
#include "core/resource/storage_resource.h"


QnSingleShotFileStreamreader::QnSingleShotFileStreamreader(const QnResourcePtr& resource):
    QnAbstractMediaStreamDataProvider(resource)
{
}

QnAbstractMediaDataPtr QnSingleShotFileStreamreader::getNextData()
{
    CodecID compressionType;
    QString lowerFileName = getResource()->getUrl().toLower();


    if (lowerFileName.endsWith(QLatin1String(".png")))
        compressionType = CODEC_ID_PNG;
    else if (lowerFileName.endsWith(QLatin1String(".jpeg")) || lowerFileName.endsWith(QLatin1String(".jpg")))
        compressionType = CODEC_ID_MJPEG;
    else if (lowerFileName.endsWith(QLatin1String(".tiff")) || lowerFileName.endsWith(QLatin1String(".tif")))
        compressionType = CODEC_ID_TIFF;
    else if (lowerFileName.endsWith(QLatin1String(".gif")))
        compressionType = CODEC_ID_GIF;
    else if (lowerFileName.endsWith(QLatin1String(".bmp")))
        compressionType = CODEC_ID_BMP;
    else
        return QnAbstractMediaDataPtr();

    if (m_storage == 0)
        m_storage = QnStorageResourcePtr(QnStoragePluginFactory::instance()->createStorage(getResource()->getUrl()));
    QIODevice* file = m_storage->open(getResource()->getUrl(), QIODevice::ReadOnly);
    if (file == 0)
        return QnAbstractMediaDataPtr();

    QByteArray srcData = file->readAll();
    QnWritableCompressedVideoDataPtr outData(new QnWritableCompressedVideoData(CL_MEDIA_ALIGNMENT, srcData.size()));
    outData->m_data.write(srcData);

    outData->compressionType = compressionType;
    outData->flags |= QnAbstractMediaData::MediaFlags_AVKey | QnAbstractMediaData::MediaFlags_StillImage;
    outData->timestamp = qnSyncTime->currentMSecsSinceEpoch()*1000;
    outData->dataProvider = this;
    outData->channelNumber = 0;

    delete file;
    return outData;
}

void QnSingleShotFileStreamreader::run()
{
    initSystemThreadId();
    QnAbstractMediaDataPtr data;
    try {
        data = getNextData();
    } catch(...) {
        qWarning() << "Application out of memory";
    }
    if (data)
        putData(std::move(data));
}

void QnSingleShotFileStreamreader::setStorage(const QnStorageResourcePtr& storage)
{
    m_storage = storage;
}

CameraDiagnostics::Result QnSingleShotFileStreamreader::diagnoseMediaStreamConnection()
{
    //TODO/IMPL
    return CameraDiagnostics::NotImplementedResult();
}

#endif // ENABLE_ARCHIVE
