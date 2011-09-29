#include "spush_media_stream_provider.h"
#include "common/sleep.h"



QnServerPushDataProvider::QnServerPushDataProvider(QnResourcePtr res):
QnAbstractMediaStreamDataProvider(res)
{
}

void QnServerPushDataProvider::sleepIfNeeded()
{
    // does nothing in spush model
}

void QnServerPushDataProvider::afterRun()
{
    if (isStreamOpened())
        closeStream();
}

bool QnServerPushDataProvider::beforeGetData()
{
    if (!isStreamOpened())
    {
        openStream();
        if (!isStreamOpened())
        {
            QnSleep::msleep(20); // to avoid large CPU usage

            setNeedKeyData();
            ++mFramesLost;
            m_stat[0].onData(0);
            m_stat[0].onEvent(CL_STAT_FRAME_LOST);

            if (mFramesLost == 4) // if we lost 2 frames => connection is lost for sure (2)
                m_stat[0].onLostConnection();

            return false;
        }
    }

    return true;
}

