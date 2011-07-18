#include "av_client_pull.h"
#include "resource/resource.h"
#include "resource/resource_param.h"
#include "common/rand.h"

QnPlAVClinetPullStreamReader::QnPlAVClinetPullStreamReader(QnResource* dev ):
QnClientPullStreamProvider(dev)
{
	setQuality(m_qulity);

	if (m_streamParam.exists("streamID"))
		m_streamParam.get("streamID").value.value = (int)cl_get_random_val(1, 32000);

	//========this is due to bug in AV firmware;
	// you cannot set up maxSensorWidth with HTTP. ( you can do it in tftp if you are really want to ). 
	// for now(13 December 2009) if we use HTTP => right should be divisible by 64; bottom - by 32
	// may be while you are looking at this comments bug already fixed.
	if (m_streamParam.exists("image_right"))
	{
		int right  = m_streamParam.get("image_right").value.value;
		right = right/64*64;
		m_streamParam.get("image_right").value.value = right;
	}

	if (m_streamParam.exists("image_bottom"))
	{
		int bottom = m_streamParam.get("image_bottom").value.value;
		bottom = bottom/32*32;
		m_streamParam.get("image_bottom").value.value = bottom;
	}
	//===================

}

QnPlAVClinetPullStreamReader::~QnPlAVClinetPullStreamReader()
{

}

void QnPlAVClinetPullStreamReader::setQuality(StreamQuality q)
{

	QnClientPullStreamProvider::setQuality(q);

	setNeedKeyData();

	QnParamList pl = getStreamParam();

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

int QnPlAVClinetPullStreamReader::getQuality() const
{
	if (!m_streamParam.exists("Quality"))
		return 10;

	if (m_qulity!=CLSHighest)
	{
		return m_streamParam.get("Quality").value.value;
	}
	else
	{
		QnValue val;
		m_device->getParam("Quality", val);
		return val;
	}

}

int QnPlAVClinetPullStreamReader::getBitrate() const
{
	if (!m_streamParam.exists("Bitrate"))
		return 0;

	if (m_qulity!=CLSHighest)
	{
		return m_streamParam.get("Bitrate").value.value;
	}
	else
	{
		QnValue val;
		m_device->getParam("Bitrate", val);
		return val;
	}

}

bool QnPlAVClinetPullStreamReader::isH264() const
{
	if (!m_streamParam.exists("Codec")) // cam is jpeg only
		return false;

	if (m_qulity!=CLSHighest)
	{
		return (m_streamParam.get("Codec").value.value == QString("H.264"));
	}
	else
	{
		QnValue val;
		m_device->getParam("Codec", val);
		return val==QString("H.264");
	}
}
