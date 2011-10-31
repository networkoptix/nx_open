#include "av_client_pull.h"
#include "../resource/av_resource.h"

QnPlAVClinetPullStreamReader::QnPlAVClinetPullStreamReader(QnResourcePtr res):
QnClientPullMediaStreamProvider(res)
{
    return;

    QnSequrityCamResourcePtr ceqResource = getResource().dynamicCast<QnSequrityCamResource>();

    QSize maxResolution = ceqResource->getMaxSensorSize();

    //========this is due to bug in AV firmware;
    // you cannot set up maxSensorWidth with HTTP. ( you can do it in tftp if you are really want to ).
    // for now(13 December 2009) if we use HTTP => right should be divisible by 64; bottom - by 32
    // may be while you are looking at this comments bug already fixed.

    /*

    maxResolution.rwidth() = maxResolution.width()/64*64;
    maxResolution.rheight() = maxResolution.height()/32*32;

    m_streamParam.put(QnParam("image_left", 0));
    m_streamParam.put(QnParam("image_right", maxResolution.width()));

    m_streamParam.put(QnParam("image_top", 0));
    m_streamParam.put(QnParam("image_bottom", maxResolution.height()));

    m_streamParam.put(QnParam("streamID", (int)cl_get_random_val(1, 32000)));

    m_streamParam.put(QnParam("resolution", "full"));
    m_streamParam.put(QnParam("Quality", "11"));

    /**/


    setQuality(m_qulity);



}

QnPlAVClinetPullStreamReader::~QnPlAVClinetPullStreamReader()
{

}

void QnPlAVClinetPullStreamReader::updateStreamParamsBasedOnQuality()
{

    return;

    //QMutexLocker mtx(&m_mtx);

    QnPlAreconVisionResourcePtr avRes = getResource().dynamicCast<QnPlAreconVisionResource>();
    QnStreamQuality q = getQuality();


	switch(q)
	{
	case QnQualityHighest:
		if (avRes->isPanoramic())
			avRes->setParamAsynch("resolution", "full", QnDomainPhysical);
		else
			m_streamParam.get("resolution").setValue("full");


        if (avRes->isPanoramic())
            avRes->setParamAsynch("Quality", "17", QnDomainPhysical); // panoramic
        else
            m_streamParam.get("Quality").setValue("15");
        break;

    case QnQualityHigh:
        if (avRes->isPanoramic())
            avRes->setParamAsynch("resolution", "full", QnDomainPhysical);
        else
            m_streamParam.get("resolution").setValue("full");


        if (avRes->isPanoramic())
            avRes->setParamAsynch("Quality", "11", QnDomainPhysical); // panoramic
        else
            m_streamParam.get("Quality").setValue("7");

        break;

    case QnQualityNormal:

        if (avRes->isPanoramic())
            avRes->setParamAsynch("resolution", "half", QnDomainPhysical);
        else
            m_streamParam.get("resolution").setValue("half");


        if (avRes->isPanoramic())
            avRes->setParamAsynch("Quality", "15", QnDomainPhysical); // panoramic
        else
            m_streamParam.get("Quality").setValue("13");

        break;

	case QnQualityLow:
	case QnQualityLowest:

        if (avRes->isPanoramic())
            avRes->setParamAsynch("resolution", "half", QnDomainPhysical);
        else
            m_streamParam.get("resolution").setValue("half");


        if (avRes->isPanoramic())
            avRes->setParamAsynch("Quality", "11", QnDomainPhysical); // panoramic
        else
            m_streamParam.get("Quality").setValue("6");

        break;

	default:
		break;
	}
}


int QnPlAVClinetPullStreamReader::getBitrate() const
{
    if (!getResource()->hasSuchParam("Bitrate"))
        return 0;

	QnValue val;
	getResource()->getParam("Bitrate", val, QnDomainMemory);
	return val;
}

bool QnPlAVClinetPullStreamReader::isH264() const
{
    if (!getResource()->hasSuchParam("Codec"))
        return false;

	QnValue val;
	getResource()->getParam("Codec", val, QnDomainMemory);
	return val==QString("H.264");
}
