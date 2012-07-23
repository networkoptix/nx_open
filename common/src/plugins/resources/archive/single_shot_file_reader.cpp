#include "single_shot_file_reader.h"
#include "filetypesupport.h"
#include "utils/common/synctime.h"
#include "utils/common/base.h"
#include "core/resource/storage_resource.h"

QnSingleShotFileStreamreader::QnSingleShotFileStreamreader(QnResourcePtr resource):
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
	QnCompressedVideoDataPtr outData(new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, srcData.size()));
    outData->data.write(srcData);

	outData->compressionType = compressionType;
    outData->flags |= AV_PKT_FLAG_KEY | QnAbstractMediaData::MediaFlags_StillImage;
    outData->timestamp = qnSyncTime->currentMSecsSinceEpoch()*1000;
    outData->dataProvider = this;
	outData->channelNumber = 0;

    delete file;
	return outData;
}

void QnSingleShotFileStreamreader::run()
{
    QnAbstractMediaDataPtr data = getNextData();
    if (data)
        putData(data);
}

void QnSingleShotFileStreamreader::setStorage(QnStorageResourcePtr storage)
{
    m_storage = storage;
}
