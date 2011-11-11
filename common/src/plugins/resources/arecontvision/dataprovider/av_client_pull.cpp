#include "av_client_pull.h"
#include "../resource/av_resource.h"
#include "utils/common/rand.h"

QnPlAVClinetPullStreamReader::QnPlAVClinetPullStreamReader(QnResourcePtr res):
QnClientPullMediaStreamProvider(res)
{

    QnSequrityCamResourcePtr ceqResource = getResource().dynamicCast<QnSequrityCamResource>();

    QSize maxResolution = ceqResource->getMaxSensorSize();

    //========this is due to bug in AV firmware;
    // you cannot set up maxSensorWidth with HTTP. ( you can do it in tftp if you are really want to ).
    // for now(13 December 2009) if we use HTTP => right should be divisible by 64; bottom - by 32
    // may be while you are looking at this comments bug already fixed.

    

    maxResolution.rwidth() = maxResolution.width()/64*64;
    maxResolution.rheight() = maxResolution.height()/32*32;

    m_streamParam.put("image_left", 0);
    m_streamParam.put("image_right", maxResolution.width());

    m_streamParam.put("image_top", 0);
    m_streamParam.put("image_bottom", maxResolution.height());

    m_streamParam.put("streamID", (int)cl_get_random_val(1, 32000));

    m_streamParam.put("resolution", "full");
    m_streamParam.put("Quality", "11");

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
			m_streamParam.put("resolution","full");


        if (avRes->isPanoramic())
            avRes->setParamAsynch("Quality", "17", QnDomainPhysical); // panoramic
        else
            m_streamParam.put("Quality","15");
        break;

    case QnQualityHigh:
        if (avRes->isPanoramic())
            avRes->setParamAsynch("resolution", "full", QnDomainPhysical);
        else
            m_streamParam.put("resolution","full");


        if (avRes->isPanoramic())
            avRes->setParamAsynch("Quality", "11", QnDomainPhysical); // panoramic
        else
            m_streamParam.put("Quality","7");

        break;

    case QnQualityNormal:

        if (avRes->isPanoramic())
            avRes->setParamAsynch("resolution", "half", QnDomainPhysical);
        else
            m_streamParam.put("resolution","half");


        if (avRes->isPanoramic())
            avRes->setParamAsynch("Quality", "15", QnDomainPhysical); // panoramic
        else
            m_streamParam.put("Quality","13");

        break;

	case QnQualityLow:
	case QnQualityLowest:

        if (avRes->isPanoramic())
            avRes->setParamAsynch("resolution", "half", QnDomainPhysical);
        else
            m_streamParam.put("resolution","half");


        if (avRes->isPanoramic())
            avRes->setParamAsynch("Quality", "11", QnDomainPhysical); // panoramic
        else
            m_streamParam.put("Quality","6");

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
