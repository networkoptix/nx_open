#include "av_client_pull.h"
#include "streamreader\streamreader.h"
#include "device\device.h"

CLAVClinetPullStreamReader::CLAVClinetPullStreamReader(CLDevice* dev ):
CLClientPullStreamreader(dev)
{
	setQuality(m_qulity);
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


		if (pl.exists("resolution"))
			pl.get("resolution").value.value = "full";
		else
			m_device->setParam_asynch("resolution", "full");


		if (pl.exists("Quality"))
			pl.get("Quality").value.value = "15";
		else
			m_device->setParam_asynch("Quality", "17"); // panoramic



		break;

	case CLSHigh:

		if (pl.exists("resolution"))
			pl.get("resolution").value.value = "full";
		else
			m_device->setParam_asynch("resolution", "full");
		

		if (pl.exists("Quality"))
			pl.get("Quality").value.value = "7";
		else
			m_device->setParam_asynch("Quality", "11"); // panoramic

	

		break;


	case CLSNormal:

		if (pl.exists("resolution"))
			pl.get("resolution").value.value = "half";
		else
			m_device->setParam_asynch("resolution", "half");


		if (pl.exists("Quality"))
			pl.get("Quality").value.value = "7";
		else
			m_device->setParam_asynch("Quality", "11");

	    break;


	case CLSLow:
	case CLSLowest:

		if (pl.exists("resolution"))
			pl.get("resolution").value.value = "half";
		else
			m_device->setParam_asynch("resolution", "half");


		if (pl.exists("Quality"))
			pl.get("Quality").value.value = "6";
		else
			m_device->setParam_asynch("Quality", "11");

	    break;
	}

	setNeedKeyData();

	setStreamParams(pl);


}

int CLAVClinetPullStreamReader::getQuality() const
{
	if (!m_streamParam.exists("Quality"))
		return 10;

	if (m_qulity!=CLSHighest)
	{
		return m_streamParam.get("Quality").value.value;
	}
	else
	{
		CLValue val;
		m_device->getParam("Quality", val);
		return val;
	}

}

int CLAVClinetPullStreamReader::getBitrate() const
{
	if (!m_streamParam.exists("Bitrate"))
		return 0;

	if (m_qulity!=CLSHighest)
	{
		return m_streamParam.get("Bitrate").value.value;
	}
	else
	{
		CLValue val;
		m_device->getParam("Bitrate", val);
		return val;
	}

}

bool CLAVClinetPullStreamReader::isH264() const
{
	if (!m_streamParam.exists("Codec")) // cam is jpeg only
		return false;

	if (m_qulity!=CLSHighest)
	{
		return (m_streamParam.get("Codec").value.value == QString("H.264"));
	}
	else
	{
		CLValue val;
		m_device->getParam("Codec", val);
		return val==QString("H.264");
	}
}