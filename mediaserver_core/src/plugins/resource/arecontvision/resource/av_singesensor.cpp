#ifdef ENABLE_ARECONT

#include "av_singesensor.h"

#include <nx/utils/log/log.h>

#include "../dataprovider/av_rtsp_stream_reader.h"
#include "../dataprovider/cpul_tftp_dataprovider.h"


CLArecontSingleSensorResource::CLArecontSingleSensorResource(const QString& name)
{
    setName(name);
}

bool CLArecontSingleSensorResource::getDescription()
{

    char buff[65];
    buff[64] = 0;

    for(int i = 0; i < 32; i++)
    {
        int val;

        if (getRegister(3,143+i,val)!=CL_HTTP_SUCCESS)
            return false;

        buff[2*i] = (val >> 8) & 0xFF;
        buff[2*i+1] = val & 0xFF; // 31*2+1 = 63

        if(buff[2*i] == 0 || buff[2*i+1]==0)
            break;
    }

    m_description = QLatin1String(buff);

    return true;
}

QnAbstractStreamDataProvider* CLArecontSingleSensorResource::createLiveDataProvider()
{
    if (isRTSPSupported())
    {
        NX_LOG(lit("Arecont. Creating live RTSP provider for camera %1").arg(getHostAddress()), cl_logDEBUG1);
        return new QnArecontRtspStreamReader(toSharedPointer());
    }
    else
    {
        NX_LOG(lit("Arecont. Create live TFTP provider for camera %1").arg(getHostAddress()), cl_logDEBUG1);
        return new AVClientPullSSTFTPStreamreader(toSharedPointer());
    }
}

#endif
