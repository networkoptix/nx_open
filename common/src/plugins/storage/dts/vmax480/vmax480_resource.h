#ifndef vmax480_resource_2047_h
#define vmax480_resource_2047_h

#include "core/resource/camera_resource.h"

class QnPlVmax480Resource : public QnPhysicalCameraResource
{
    Q_OBJECT

public:
    static const char* MANUFACTURE;

    QnPlVmax480Resource();

    virtual int getMaxFps() override; 
    virtual bool isResourceAccessible() override;
    virtual bool updateMACAddress() override;
    virtual QString manufacture() const override;
    virtual void setIframeDistance(int frames, int timems) override; // sets the distance between I frames


    virtual QString getHostAddress() const override;
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

protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual QnAbstractStreamDataProvider* createArchiveDataProvider() override;

    virtual void setCropingPhysical(QRect croping) override;
    virtual bool initInternal() override;
private:
    qint64 m_startTime;
    qint64 m_endTime;
};

typedef QnSharedResourcePointer<QnPlVmax480Resource> QnPlVmax480ResourcePtr;

#endif //vmax480_resource_2047_h