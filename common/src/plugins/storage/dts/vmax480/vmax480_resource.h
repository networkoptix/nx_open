#ifndef vmax480_resource_2047_h
#define vmax480_resource_2047_h

#include "core/resource/camera_resource.h"
#include "recording/time_period_list.h"

class QnVMax480ChunkReader;
class QnAbstractArchiveDelegate;

class QnPlVmax480Resource : public QnPhysicalCameraResource
{
    Q_OBJECT

public:
    static const char* MANUFACTURE;

    QnPlVmax480Resource();
    virtual ~QnPlVmax480Resource();

    virtual int getMaxFps() override; 
    virtual bool isResourceAccessible() override;
    virtual QString manufacture() const override;
    virtual void setIframeDistance(int frames, int timems) override; // sets the distance between I frames

    virtual bool setHostAddress(const QString &ip, QnDomain domain) override;
    virtual bool shoudResolveConflicts() const override;

    int channelNum() const;
    int videoPort() const;
    int eventPort() const;

    virtual bool hasDualStreaming() const override { return false; }

    void setStartTime(qint64 valueUsec);
    qint64 startTime() const;

    void setEndTime(qint64 valueUsec);
    qint64 endTime() const;

    void setArchiveRange(qint64 startTimeUsec, qint64 endTimeUsec);

    virtual void setStatus(Status newStatus, bool silenceMode = false);

    virtual QnTimePeriodList getDtsTimePeriods(qint64 startTimeMs, qint64 endTimeMs, int detailLevel) override;
    virtual QnAbstractArchiveDelegate* createArchiveDelegate() override;
    QnTimePeriodList getChunks();
protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

    virtual void setCropingPhysical(QRect croping) override;
    virtual bool initInternal() override;
    void setChunks(const QnTimePeriodList& chunks);
    QnPhysicalCameraResourcePtr getOtherResource(int channel);
private slots:
    void at_gotChunks(int channel, QnTimePeriodList chunks);
private:
    qint64 m_startTime;
    qint64 m_endTime;
    QnVMax480ChunkReader* m_chunkReader;
    QnTimePeriodList m_chunks;
    
    QMutex m_mutexChunks;
    //QWaitCondition m_chunksCond;
    bool m_chunksReady;
};

typedef QnSharedResourcePointer<QnPlVmax480Resource> QnPlVmax480ResourcePtr;

#endif //vmax480_resource_2047_h