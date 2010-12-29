#include "./single_shot_file_reader.h"
#include "../data/mediadata.h"
#include "../device/file_device.h"
#include <QFileInfo>
#include <QFile>

CLSingleShotFileStreamreader::CLSingleShotFileStreamreader(CLDevice* dev ):
CLSingleShotStreamreader(dev)
{
	CLFileDevice* device = static_cast<CLFileDevice*>(dev);
	m_fileName = device->getFileName();
}


CLAbstractMediaData* CLSingleShotFileStreamreader::getData()
{
	QFileInfo finfo(m_fileName);
	QString extention = finfo.suffix().toLower();

	if (extention!="jpeg" && extention!="jpg") // so far we support only these
		return 0;

	QFile file(m_fileName);
	if (!file.exists())
		return 0;

	if (!file.open(QIODevice::ReadOnly))
		return 0;

	unsigned int file_size = file.size();

	CLCompressedVideoData* outData = new CLCompressedVideoData(CL_MEDIA_ALIGNMENT,file_size);
	CLByteArray& data = outData->data;

	data.prepareToWrite(file_size);

	QDataStream in(&file);
	int readed = in.readRawData(data.data(), file_size);

	data.done(readed);


	outData->compressionType = CL_JPEG;

	outData->width = 0; // does not really meter (this is single shot)
	outData->height = 0; //does not really meter (this is single shot)
	outData->keyFrame = true;

	outData->channel_num = 0;

	return outData;
}
