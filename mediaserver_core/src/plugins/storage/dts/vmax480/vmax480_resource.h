#pragma once

#ifdef ENABLE_VMAX

#include <nx/mediaserver/resource/camera.h>
#include <recording/time_period_list.h>

class QnVMax480ChunkReader;
class QnAbstractArchiveDelegate;

class QnPlVmax480Resource: public nx::mediaserver::resource::Camera
{
    Q_OBJECT

public:
    static const QString MANUFACTURE;

    QnPlVmax480Resource();
    virtual ~QnPlVmax480Resource();

    virtual int getMaxFps() const override;
    virtual QString getDriverName() const override;
    virtual void setIframeDistance(int frames, int timems) override; // sets the distance between I frames

    virtual void setHostAddress(const QString &ip) override;

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

protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

    virtual CameraDiagnostics::Result initializeCameraDriver() override;
    void setChunks(const QnTimePeriodList& chunks);
    QnSecurityCamResourcePtr getOtherResource(int channel);
    virtual Qn::LicenseType calculateLicenseType() const override;

private slots:
    void at_gotChunks(int channel, QnTimePeriodList chunks);
private:
    qint64 m_startTime;
    qint64 m_endTime;
    QnVMax480ChunkReader* m_chunkReader;
    QString m_chunkReaderKey;
    QnTimePeriodList m_chunks;

    QnMutex m_mutexChunks;
    //QnWaitCondition m_chunksCond;
    bool m_chunksReady;

    static QnMutex m_chunkReaderMutex;
    static QMap<QString, QnVMax480ChunkReader*> m_chunkReaderMap;
};

typedef QnSharedResourcePointer<QnPlVmax480Resource> QnPlVmax480ResourcePtr;

#endif // #ifdef ENABLE_VMAX
