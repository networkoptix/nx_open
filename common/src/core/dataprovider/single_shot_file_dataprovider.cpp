#include "../resource/local_file_resource.h"
#include "plugins/resources/archive/filetypesupport.h"
#include "single_shot_file_dataprovider.h"


CLSingleShotFileStreamreader::CLSingleShotFileStreamreader(QnResourcePtr dev ):
CLSingleShotStreamreader(dev)
{
	QnLocalFileResourcePtr device = qSharedPointerDynamicCast<QnLocalFileResource>(dev);
	m_fileName = device->getFileName();
}

QnAbstractMediaDataPtr CLSingleShotFileStreamreader::getData()
{
	FileTypeSupport fileTypeSupport;

	if (!fileTypeSupport.isImageFileExt(m_fileName))
		return QnAbstractMediaDataPtr();

	QFile file(m_fileName);
	if (!file.exists())
		return QnAbstractMediaDataPtr();

	if (!file.open(QIODevice::ReadOnly))
		return QnAbstractMediaDataPtr();

	CodecID compressionType;

    QString lowerFileName = m_fileName.toLower();

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

    unsigned int file_size = file.size();

	QnCompressedVideoDataPtr outData(new QnCompressedVideoData(CL_MEDIA_ALIGNMENT,file_size));
	CLByteArray& data = outData->data;

	data.prepareToWrite(file_size);

	QDataStream in(&file);
	int readed = in.readRawData(data.data(), file_size);

	data.done(readed);

	outData->compressionType = compressionType;

	outData->width = 0; // does not really meter (this is single shot)
	outData->height = 0; //does not really meter (this is single shot)
	//outData->keyFrame = true;
    outData->flags |= AV_PKT_FLAG_KEY;

	outData->channelNumber = 0;

	return outData;
}
