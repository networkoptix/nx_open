#include "single_shot_file_dataprovider.h"
#include "plugins/resources/archive/filetypesupport.h"
#include "resource/file_resource.h"
#include "common/bytearray.h"
#include "datapacket/mediadatapacket.h"
#include "common/base.h"


CLSingleShotFileStreamreader::CLSingleShotFileStreamreader(CLDevice* dev ):
CLSingleShotStreamreader(dev)
{
	CLFileDevice* device = static_cast<CLFileDevice*>(dev);
	m_fileName = device->getFileName();
}

CLAbstractMediaData* CLSingleShotFileStreamreader::getData()
{
    FileTypeSupport fileTypeSupport;

	if (!fileTypeSupport.isImageFileExt(m_fileName))
		return 0;

	QFile file(m_fileName);
	if (!file.exists())
		return 0;

	if (!file.open(QIODevice::ReadOnly))
		return 0;

    CodecID compressionType;

    QString lowerFileName = m_fileName.toLower();
    
    if (lowerFileName.endsWith(".png"))
        compressionType = CODEC_ID_PNG;
    else if (lowerFileName.endsWith(".jpeg") || lowerFileName.endsWith(".jpg"))
        compressionType = CODEC_ID_MJPEG;
    else if (lowerFileName.endsWith(".tiff") || lowerFileName.endsWith(".tif"))
        compressionType = CODEC_ID_TIFF;
    else if (lowerFileName.endsWith(".gif"))
        compressionType = CODEC_ID_GIF;
    else if (lowerFileName.endsWith(".bmp"))
        compressionType = CODEC_ID_BMP;
    else
        return 0;

	unsigned int file_size = file.size();

	CLCompressedVideoData* outData = new CLCompressedVideoData(CL_MEDIA_ALIGNMENT,file_size);
	CLByteArray& data = outData->data;

	data.prepareToWrite(file_size);

	QDataStream in(&file);
	int readed = in.readRawData(data.data(), file_size);

	data.done(readed);

    outData->compressionType = compressionType;

	outData->width = 0; // does not really meter (this is single shot)
	outData->height = 0; //does not really meter (this is single shot)
	outData->keyFrame = true;

	outData->channelNumber = 0;

	return outData;
}
