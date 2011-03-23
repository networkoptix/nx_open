#include "stream_recorder.h"

#include "device/device.h"
#include "base/bytearray.h"
#include "data/mediadata.h"
#include "streamreader/streamreader.h"
#include "util.h"

#define FLUSH_SIZE (512*1024)

CLStreamRecorder::CLStreamRecorder(CLDevice* dev):
CLAbstractDataProcessor(10),
m_device(dev),
m_firstTime(true)
{
	m_version = 1;

	memset(m_data, 0, sizeof(m_data));
	memset(m_description, 0, sizeof(m_description));

	memset(m_gotKeyFrame, 0, sizeof(m_gotKeyFrame)); // false

	memset(m_dataFile, 0, sizeof(m_dataFile));
	memset(m_descriptionFile, 0, sizeof(m_descriptionFile));
}

CLStreamRecorder::~CLStreamRecorder()
{
	cleanup();
}

void CLStreamRecorder::cleanup()
{
	for (int i = 0; i < CL_MAX_CHANNELS; ++i)
	{
		flushChannel(i);

		delete m_data[i];
		m_data[i] = 0;

		delete m_description[i];
		m_description[i] = 0;

		m_gotKeyFrame[i] = false;

		if (m_dataFile[i])
		{
			fclose(m_dataFile[i]);
			m_dataFile[i] = 0;
		}

		if (m_descriptionFile[i])
		{
			fclose(m_descriptionFile[i]);
			m_descriptionFile[i] = 0;
		}
	}

	m_firstTime = true;

	memset(m_data, 0, sizeof(m_data));
	memset(m_description, 0, sizeof(m_description));

	memset(m_gotKeyFrame, 0, sizeof(m_gotKeyFrame)); // false

	memset(m_dataFile, 0, sizeof(m_dataFile));
	memset(m_descriptionFile, 0, sizeof(m_descriptionFile));
}

void CLStreamRecorder::processData(CLAbstractData* data)
{
	if (m_firstTime)
	{
		m_firstTime = false;
		onFirstData(data);
	}

	CLAbstractMediaData* md = static_cast<CLAbstractMediaData*>(data);
	if (md->dataType != CLAbstractMediaData::VIDEO)
		return;

	CLCompressedVideoData *vd= static_cast<CLCompressedVideoData*>(data);
	int channel = vd->channelNumber;

	if (channel>CL_MAX_CHANNELS-1)
		return;

	bool keyFrame = vd->keyFrame;

	if (keyFrame)
		m_gotKeyFrame[channel] = true;

	if (!m_gotKeyFrame[channel]) // did not got key frame so far
		return;

	if (m_data[channel]==0)
		m_data[channel] = new CLByteArray(0, 1024*1024);

	if (m_description[channel]==0)
	{
		m_description[channel] = new CLByteArray(0, 1024);
		m_description[channel]->write((char*)(&m_version), 4); // version
	}

	quint64 t64 = vd->timestamp;

	unsigned int data_size = vd->data.size();
	unsigned int codec = (unsigned int)vd->compressionType;

	m_description[channel]->write((char*)&data_size,4);
	m_description[channel]->write((char*)&t64,8);
	m_description[channel]->write((char*)&codec,4);
	m_description[channel]->write((char*)&keyFrame,1);

	m_data[channel]->write(vd->data);

	if (needToFlush(channel))
		flushChannel(channel);
}

void CLStreamRecorder::endOfRun()
{
	cleanup();
}

void CLStreamRecorder::onFirstData(CLAbstractData* data)
{
	QDir current_dir;
	current_dir.mkpath(dirHelper());

	CLStreamreader* reader = data->dataProvider;
	CLDevice* dev = reader->getDevice();

	QDomDocument doc("");
	QDomElement root = doc.createElement("layout");
	doc.appendChild(root);

	root.setAttribute("channels", dev->getVideoLayout()->numberOfChannels());
	root.setAttribute("width", dev->getVideoLayout()->width());
	root.setAttribute("height", dev->getVideoLayout()->height());

    

    root.setAttribute("name", dev->getName() + QDateTime::currentDateTime().toString(" ddd MMM d yyyy  hh_mm"));

	for (int i = 0; i < dev->getVideoLayout()->numberOfChannels(); ++i)
	{
		QDomElement channel = doc.createElement("channel");

		channel.setAttribute("number", i);
		channel.setAttribute("h_pos", dev->getVideoLayout()->h_position(i));
		channel.setAttribute("v_pos", dev->getVideoLayout()->v_position(i));

		root.appendChild(channel);
	}

	QString xml = doc.toString();

	QFile file(dirHelper() + "/layout.xml");
	file.open(QIODevice::WriteOnly);

	QTextStream fstr(&file);
	fstr<< xml;
	fstr.flush();	
}

//=====================================
bool CLStreamRecorder::needToFlush(int channel)
{
	if (m_description[channel]==0 || m_data[channel]==0 || m_description[channel]->size()==0 || m_data[channel]->size()==0)
		return false;

	if (m_data[channel]->size()>=FLUSH_SIZE)
		return true;

	return false;
}

void CLStreamRecorder::flushChannel(int channel)
{
	if (m_description[channel]==0 || m_data[channel]==0 || m_description[channel]->size()==0 || m_data[channel]->size()==0)
		return;

	if (m_descriptionFile[channel]==0)
		m_descriptionFile[channel] = fopen(filenameDescription(channel).toLatin1().data(), "wb");

	if (m_descriptionFile[channel]==0)
		return;

	if (m_dataFile[channel]==0)
		m_dataFile[channel] = fopen(filenameData(channel).toLatin1().data(), "wb");

	if (m_dataFile[channel]==0)
		return;

	fwrite(m_description[channel]->data(),1,m_description[channel]->size(),m_descriptionFile[channel]);
	fflush(m_descriptionFile[channel]);
	m_description[channel]->clear();

	fwrite(m_data[channel]->data(),1,m_data[channel]->size(),m_dataFile[channel]);
	fflush(m_dataFile[channel]);
	m_data[channel]->clear();

}

QString CLStreamRecorder::filenameData(int channel)
{
	return filenameHelper(channel) + ".data";
}

QString CLStreamRecorder::filenameDescription(int channel)
{
	return filenameHelper(channel) + ".descr";
}

QString CLStreamRecorder::filenameHelper(int channel)
{
	QString str;
	QTextStream stream(&str);

	stream << dirHelper() << "/channel_" << channel;

	return str;
}

QString CLStreamRecorder::dirHelper()
{
	QString str;
	QTextStream stream(&str);


	stream << getTempRecordingDir() << m_device->getUniqueId();
	return str;
}
