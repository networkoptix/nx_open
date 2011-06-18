#include "./single_shot_file_reader.h"
#include "../data/mediadata.h"
#include "../device/file_device.h"
#include "device_plugins/archive/filetypesupport.h"

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

    QByteArray imageFormat = QImageReader::imageFormat(&file);

    CodecID compressionType;

    if (imageFormat == "png")
        compressionType = CODEC_ID_PNG;
    else if (imageFormat == "jpeg")
        compressionType = CODEC_ID_MJPEG;
    else if (imageFormat == "tiff")
        compressionType = CODEC_ID_TIFF;
    else if (imageFormat == "gif")
        compressionType = CODEC_ID_GIF;
    else if (imageFormat == "bmp")
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
