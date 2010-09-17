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


		if (pl.exists("quality"))
			pl.get("quality").value.value = "15";
		else
			m_device->setParam_asynch("quality", "17"); // panoramic



		break;

	case CLSHigh:

		if (pl.exists("resolution"))
			pl.get("resolution").value.value = "full";
		else
			m_device->setParam_asynch("resolution", "full");
		

		if (pl.exists("quality"))
			pl.get("quality").value.value = "7";
		else
			m_device->setParam_asynch("quality", "11"); // panoramic

	

		break;


	case CLSNormal:

		if (pl.exists("resolution"))
			pl.get("resolution").value.value = "half";
		else
			m_device->setParam_asynch("resolution", "half");


		if (pl.exists("quality"))
			pl.get("quality").value.value = "7";
		else
			m_device->setParam_asynch("quality", "11");

	    break;


	case CLSLow:
	case CLSLowest:

		if (pl.exists("resolution"))
			pl.get("resolution").value.value = "half";
		else
			m_device->setParam_asynch("resolution", "half");


		if (pl.exists("quality"))
			pl.get("quality").value.value = "6";
		else
			m_device->setParam_asynch("quality", "11");

	    break;
	}

	setNeedKeyData();

	setStreamParams(pl);


}