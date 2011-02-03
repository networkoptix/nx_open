#include "stream_recorder.h"

#include "device\device.h"
#include "base\bytearray.h"
#include "data\mediadata.h"
#include "streamreader\streamreader.h"

#define FLUSH_SIZE (512*1024)

CLStreamRecorder::CLStreamRecorder(CLDevice* dev):
CLAbstractDataProcessor(10),
mDevice(dev),
mFirsTime(true)
{
	mVersion = 1;

	memset(mData, 0, sizeof(mData));
	memset(mDescr, 0, sizeof(mDescr));

	memset(mGotKeyFrame, 0, sizeof(mGotKeyFrame)); // false

	memset(mDataFile, 0, sizeof(mDataFile));
	memset(mDescrFile, 0, sizeof(mDescrFile));
	

}

CLStreamRecorder::~CLStreamRecorder()
{
	cleanup();
}

void CLStreamRecorder::cleanup()
{
	for (int i = 0; i < CL_MAX_CHANNELS; ++i)
	{
		flush_channel(i);

		delete mData[i];
		mData[i] = 0;

		delete mDescr[i];
		mDescr[i] = 0;

		mGotKeyFrame[i] = false;

		if (mDataFile[i])
		{
			fclose(mDataFile[i]);
			mDataFile[i] = 0;
		}

		if (mDescrFile[i])
		{
			fclose(mDescrFile[i]);
			mDescrFile[i] = 0;
		}


	}
	
	mFirsTime = true;

	memset(mData, 0, sizeof(mData));
	memset(mDescr, 0, sizeof(mDescr));

	memset(mGotKeyFrame, 0, sizeof(mGotKeyFrame)); // false

	memset(mDataFile, 0, sizeof(mDataFile));
	memset(mDescrFile, 0, sizeof(mDescrFile));
}

void CLStreamRecorder::processData(CLAbstractData* data)
{
	if (mFirsTime)
	{
		mFirsTime = false;
		onFirstdata(data);
	}

	CLAbstractMediaData* md = static_cast<CLAbstractMediaData*>(data);
	if (md->dataType != CLAbstractMediaData::VIDEO)
		return;

	CLCompressedVideoData *vd= static_cast<CLCompressedVideoData*>(data);
	int channel = vd->channel_num;

	if (channel>CL_MAX_CHANNELS-1)
		return;

	bool keyFrame = vd->keyFrame;

	if (keyFrame)
		mGotKeyFrame[channel] = true;

	if (!mGotKeyFrame[channel]) // did not got key frame so far
		return;

	if (mData[channel]==0)
		mData[channel] = new CLByteArray(0, 1024*1024);

	if (mDescr[channel]==0)
	{
		mDescr[channel] = new CLByteArray(0, 1024);
		mDescr[channel]->write((char*)(&mVersion), 4); // version
	}

	
	quint64 t64 = vd->timestamp;

	unsigned int data_size = vd->data.size();
	unsigned int codec = (unsigned int)vd->compressionType;
	

	mDescr[channel]->write((char*)&data_size,4);
	mDescr[channel]->write((char*)&t64,8);
	mDescr[channel]->write((char*)&codec,4);
	mDescr[channel]->write((char*)&keyFrame,1);

	mData[channel]->write(vd->data);

	if (need_to_flush(channel))
		flush_channel(channel);
}

void CLStreamRecorder::endOfRun()
{
	cleanup();
}

void CLStreamRecorder::onFirstdata(CLAbstractData* data)
{
	QDir current_dir;
	current_dir.mkpath(dir_helper());

	CLStreamreader* reader = reinterpret_cast<CLStreamreader*>(data->dataProvider);
	CLDevice* dev = reader->getDevice();

	QDomDocument doc("");
	QDomElement root = doc.createElement("layout");
	doc.appendChild(root);


	root.setAttribute("channels", dev->getVideoLayout()->numberOfChannels());
	root.setAttribute("width", dev->getVideoLayout()->width());
	root.setAttribute("height", dev->getVideoLayout()->height());


	for (int i = 0; i < dev->getVideoLayout()->numberOfChannels(); ++i)
	{
		QDomElement channel = doc.createElement("channel");

		channel.setAttribute("number", i);
		channel.setAttribute("h_pos", dev->getVideoLayout()->h_position(i));
		channel.setAttribute("v_pos", dev->getVideoLayout()->v_position(i));

		root.appendChild(channel);
	}


	QString xml = doc.toString();

	QFile file(dir_helper() + "/layout.xml");
	file.open(QIODevice::WriteOnly);

	QTextStream fstr(&file);
	fstr<< xml;
	fstr.flush();	
}

//=====================================
bool CLStreamRecorder::need_to_flush(int channel)
{
	if (mDescr[channel]==0 || mData[channel]==0 || mDescr[channel]->size()==0 || mData[channel]->size()==0)
		return false;

	if (mData[channel]->size()>=FLUSH_SIZE)
		return true;

	return false;
}


void CLStreamRecorder::flush_channel(int channel)
{
	
	
	if (mDescr[channel]==0 || mData[channel]==0 || mDescr[channel]->size()==0 || mData[channel]->size()==0)
		return;

	if (mDescrFile[channel]==0)
		mDescrFile[channel] = fopen(filname_descr(channel).toLatin1().data(), "wb");

	if (mDescrFile[channel]==0)
		return;


	if (mDataFile[channel]==0)
		mDataFile[channel] = fopen(filname_data(channel).toLatin1().data(), "wb");

	if (mDataFile[channel]==0)
		return;

	fwrite(mDescr[channel]->data(),1,mDescr[channel]->size(),mDescrFile[channel]);
	fflush(mDescrFile[channel]);
	mDescr[channel]->clear();

	fwrite(mData[channel]->data(),1,mData[channel]->size(),mDataFile[channel]);
	fflush(mDataFile[channel]);
	mData[channel]->clear();



}

QString CLStreamRecorder::filname_data(int channel)
{
	return filname_helper(channel) + ".data";
}

QString CLStreamRecorder::filname_descr(int channel)
{
	return filname_helper(channel) + ".descr";
}

QString CLStreamRecorder::filname_helper(int channel)
{
	
	QString str;
	QTextStream stream(&str);

	stream << dir_helper() << "/channel_" << channel;

	return str;
}

QString CLStreamRecorder::dir_helper()
{
	QString str;
	QTextStream stream(&str);

	stream << "./archive/" << mDevice->getUniqueId();

	return str;
}