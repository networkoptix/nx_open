#include "av_client_pull.h"
#include "streamreader/streamreader.h"
#include "device/device.h"
#include "base/rand.h"

CLAVClinetPullStreamReader::CLAVClinetPullStreamReader(CLDevice* dev ):
CLClientPullStreamreader(dev)
{
	setQuality(m_qulity);

	if (m_streamParam.exists(QLatin1String("streamID")))
		m_streamParam.get(QLatin1String("streamID")).value.value = (int)cl_get_random_val(1, 32000);

	//========this is due to bug in AV firmware;
	// you cannot set up maxSensorWidth with HTTP. ( you can do it in tftp if you are really want to ).
	// for now(13 December 2009) if we use HTTP => right should be divisible by 64; bottom - by 32
	// may be while you are looking at this comments bug already fixed.
	if (m_streamParam.exists(QLatin1String("image_right")))
	{
		int right  = m_streamParam.get(QLatin1String("image_right")).value.value;
		right = right/64*64;
		m_streamParam.get(QLatin1String("image_right")).value.value = right;
	}

	if (m_streamParam.exists(QLatin1String("image_bottom")))
	{
		int bottom = m_streamParam.get(QLatin1String("image_bottom")).value.value;
		bottom = bottom/32*32;
		m_streamParam.get(QLatin1String("image_bottom")).value.value = bottom;
	}
	//===================

}

CLAVClinetPullStreamReader::~CLAVClinetPullStreamReader()
{

}

void CLAVClinetPullStreamReader::setQuality(StreamQuality q)
{

	CLClientPullStreamreader::setQuality(q);

	setNeedKeyData();

	CLParamList pl = getStreamParam();

	switch(q)
	{
	case CLSHighest:

		if (pl.exists(QLatin1String("resolution")))
			pl.get(QLatin1String("resolution")).value.value = "full";
		else
			m_device->setParam_asynch(QLatin1String("resolution"), "full");

		if (pl.exists(QLatin1String("Quality")))
			pl.get(QLatin1String("Quality")).value.value = "15";
		else
			m_device->setParam_asynch(QLatin1String("Quality"), "17"); // panoramic

		break;

	case CLSHigh:

		if (pl.exists(QLatin1String("resolution")))
			pl.get(QLatin1String("resolution")).value.value = "full";
		else
			m_device->setParam_asynch(QLatin1String("resolution"), "full");

		if (pl.exists(QLatin1String("Quality")))
			pl.get(QLatin1String("Quality")).value.value = "7";
		else
			m_device->setParam_asynch(QLatin1String("Quality"), "11"); // panoramic

		break;

	case CLSNormal:

		if (pl.exists(QLatin1String("resolution")))
			pl.get(QLatin1String("resolution")).value.value = "half";
		else
			m_device->setParam_asynch(QLatin1String("resolution"), "half");

		if (pl.exists(QLatin1String("Quality")))
			pl.get(QLatin1String("Quality")).value.value = "7";
		else
			m_device->setParam_asynch(QLatin1String("Quality"), "11");

		break;

	case CLSLow:
	case CLSLowest:

		if (pl.exists(QLatin1String("resolution")))
			pl.get(QLatin1String("resolution")).value.value = "half";
		else
			m_device->setParam_asynch(QLatin1String("resolution"), "half");

		if (pl.exists(QLatin1String("Quality")))
			pl.get(QLatin1String("Quality")).value.value = "6";
		else
			m_device->setParam_asynch(QLatin1String("Quality"), "11");

		break;
	}

	setNeedKeyData();

	setStreamParams(pl);

}

int CLAVClinetPullStreamReader::getQuality() const
{
	if (!m_streamParam.exists(QLatin1String("Quality")))
		return 10;

	if (m_qulity!=CLSHighest)
	{
		return m_streamParam.get(QLatin1String("Quality")).value.value;
	}
	else
	{
		CLValue val;
		m_device->getParam(QLatin1String("Quality"), val);
		return val;
	}

}

int CLAVClinetPullStreamReader::getBitrate() const
{
	if (!m_streamParam.exists(QLatin1String("Bitrate")))
		return 0;

	if (m_qulity!=CLSHighest)
	{
		return m_streamParam.get(QLatin1String("Bitrate")).value.value;
	}
	else
	{
		CLValue val;
		m_device->getParam(QLatin1String("Bitrate"), val);
		return val;
	}

}

bool CLAVClinetPullStreamReader::isH264() const
{
	if (!m_streamParam.exists(QLatin1String("Codec"))) // cam is jpeg only
		return false;

	if (m_qulity!=CLSHighest)
	{
		return (m_streamParam.get(QLatin1String("Codec")).value.value == QLatin1String("H.264"));
	}
	else
	{
		CLValue val;
		m_device->getParam(QLatin1String("Codec"), val);
		return val==QLatin1String("H.264");
	}
}
