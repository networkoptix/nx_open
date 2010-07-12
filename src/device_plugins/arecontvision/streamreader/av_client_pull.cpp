#include "av_client_pull.h"
#include "streamreader\streamreader.h"
#include "device\device.h"

CLAVClinetPullStreamReader::CLAVClinetPullStreamReader(CLDevice* dev ):
CLClientPullStreamreader(dev)
{

}


CLAVClinetPullStreamReader::~CLAVClinetPullStreamReader()
{

}


void CLAVClinetPullStreamReader::setQuality(StreamQuality q)
{

	CLClientPullStreamreader::setQuality(q);


	CLParamList pl = getStreamParam();

	switch(q)
	{
	case CLSHighest:
	case CLSHigh:

		if (pl.exists("resolution"))
			pl.get("resolution").value.value = "full";
		else
			m_device->setParam_asynch("resolution", "full");
		

		if (pl.exists("quality"))
			pl.get("quality").value.value = "15";
		else
			m_device->setParam_asynch("quality", "15"); // panoramic

	

		break;


	case CLSNormal:

		if (pl.exists("resolution"))
			pl.get("resolution").value.value = "half";
		else
			m_device->setParam_asynch("resolution", "full");


		if (pl.exists("quality"))
			pl.get("quality").value.value = "10";
		else
			m_device->setParam_asynch("quality", "10");

	    break;


	case CLSLow:
	case CLSLowest:

		if (pl.exists("resolution"))
			pl.get("resolution").value.value = "half";
		else
			m_device->setParam_asynch("resolution", "full");


		if (pl.exists("quality"))
			pl.get("quality").value.value = "2";
		else
			m_device->setParam_asynch("quality", "2");

	    break;
	}

	needKeyData();

	setStreamParams(pl);


}