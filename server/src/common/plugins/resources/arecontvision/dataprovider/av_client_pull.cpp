#include "av_client_pull.h"
#include "resource/resource.h"
#include "resource/resource_param.h"
#include "common/rand.h"

QnPlAVClinetPullStreamReader::QnPlAVClinetPullStreamReader(QnResourcePtr res):
QnClientPullStreamProvider(res)
{

    m_streamParam.put(QnParam("streamID", (int)cl_get_random_val(1, 32000)));


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

void QnPlAVClinetPullStreamReader::setQuality(QnStreamQuality q)
{

	QnClientPullStreamProvider::setQuality(q);

	setNeedKeyData();

	QnParamList pl = getStreamParam();

	switch(q)
	{
	case QnQualityHighest:

		if (pl.exists("resolution"))
			pl.get("resolution").value.value = "full";
		else
			m_device->setParam_asynch("resolution", "full");

		if (pl.exists("Quality"))
			pl.get("Quality").value.value = "15";
		else
			m_device->setParam_asynch("Quality", "17"); // panoramic

		break;

	case QnQualityHigh:

		if (pl.exists("resolution"))
			pl.get("resolution").value.value = "full";
		else
			m_device->setParam_asynch("resolution", "full");

		if (pl.exists("Quality"))
			pl.get("Quality").value.value = "7";
		else
			m_device->setParam_asynch("Quality", "11"); // panoramic

		break;

	case QnQualityNormal:

		if (pl.exists("resolution"))
			pl.get("resolution").value.value = "half";
		else
			m_device->setParam_asynch("resolution", "half");

		if (pl.exists("Quality"))
			pl.get("Quality").value.value = "7";
		else
			m_device->setParam_asynch("Quality", "11");

	    break;

	case QnQualityLow:
	case QnQualityLowest:

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

void QnPlAVClinetPullStreamReader::updateStreamParamsBasedOnQuality()
{

}


int QnPlAVClinetPullStreamReader::getBitrate() const
{
    if (!getResource()->hasSuchParam("Bitrate"))
        return 0;

	QnValue val;
	getResource()->getParam("Bitrate", val);
	return val;
}

bool QnPlAVClinetPullStreamReader::isH264() const
{
    if (!getResource().hasSuchParam("Codec"))
        return false;

	QnValue val;
	getResource()->getParam("Codec", val);
	return val==QString("H.264");
}
