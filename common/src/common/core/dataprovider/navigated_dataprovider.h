#ifndef __ABSTRACT_ARCHIVE_DATAPROVIDER_H
#define __ABSTRACT_ARCHIVE_DATAPROVIDER_H

#include "dataprovider/cpull_media_stream_provider.h"

class QnNavigatedDataProvider: public QnClientPullStreamProvider
{
public:
    QnNavigatedDataProvider(QnResourcePtr res);

    void setSingleShotMode(bool single);
    bool isSingleShotMode() const;
protected:
    virtual void updateStreamParamsBasedOnQuality(); 

    virtual void channeljumpTo(quint64 mksec, int channel) = 0;
    quint64 skipFramesToTime() const;
    virtual void setSkipFramesToTime(quint64 skipFramesToTime);
    virtual quint64 startMksec() const { return m_startMksec; }
protected:
    quint64 m_lengthMksec;
    quint64 m_startMksec;
    bool m_forward;
    bool m_singleShot;
    //CLAdaptiveSleep m_adaptiveSleep;
    qint64 m_needToSleep;
    mutable QMutex m_cs;
    mutable QMutex m_framesMutex;
    bool m_useTwice;
private:
    quint64 m_skipFramesToTime;
};

#endif
