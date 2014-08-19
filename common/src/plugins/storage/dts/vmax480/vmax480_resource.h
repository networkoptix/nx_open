#ifndef vmax480_resource_2047_h
#define vmax480_resource_2047_h

#ifdef ENABLE_VMAX

#include "core/resource/camera_resource.h"
#include "recording/time_period_list.h"

class QnVMax480ChunkReader;
class QnAbstractArchiveDelegate;

class QnPlVmax480Resource : public QnPhysicalCameraResource
{
    Q_OBJECT

public:
    static const QString MANUFACTURE;

    QnPlVmax480Resource();
    virtual ~QnPlVmax480Resource();

    virtual int getMaxFps() const override;
    virtual bool isResourceAccessible() override;
    virtual QString getDriverName() const override;
    virtual void setIframeDistance(int frames, int timems) override; // sets the distance between I frames

    virtual bool setHostAddress(const QString &ip, QnDomain domain) override;

    int channelNum() const;
    int videoPort() const;
    int eventPort() const;

    virtual bool hasDualStreaming() const override { return false; }

    void setStartTime(qint64 valueUsec);
    qint64 startTime() const;

    void setEndTime(qint64 valueUsec);
    qint64 endTime() const;

    void setArchiveRange(qint64 startTimeUsec, qint64 endTimeUsec, bool recursive = true);

    virtual QnTimePeriodList getDtsTimePeriods(qint64 startTimeMs, qint64 endTimeMs, int detailLevel) override;
    virtual QnAbstractStreamDataProvider* createArchiveDataProvider() override;
    virtual QnAbstractArchiveDelegate* createArchiveDelegate() override;
    QnTimePeriodList getChunks();
    virtual Qn::LicenseType licenseType() const override;
protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

    virtual CameraDiagnostics::Result initInternal() override;
    void setChunks(const QnTimePeriodList& chunks);
    QnPhysicalCameraResourcePtr getOtherResource(int channel);
private slots:
    void at_gotChunks(int channel, QnTimePeriodList chunks);
private:
    qint64 m_startTime;
    qint64 m_endTime;
    QnVMax480ChunkReader* m_chunkReader;
    QString m_chunkReaderKey;
    QnTimePeriodList m_chunks;
    
    QMutex m_mutexChunks;
    //QWaitCondition m_chunksCond;
    bool m_chunksReady;

    static QMutex m_chunkReaderMutex;
    static QMap<QString, QnVMax480ChunkReader*> m_chunkReaderMap;
};

typedef QnSharedResourcePointer<QnPlVmax480Resource> QnPlVmax480ResourcePtr;

#endif // #ifdef ENABLE_VMAX
#endif //vmax480_resource_2047_h
