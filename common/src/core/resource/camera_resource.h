#ifndef QN_CAMERA_RESOURCE_H
#define QN_CAMERA_RESOURCE_H

#include <QtCore/QMetaType>

#include "security_cam_resource.h"
#include <deque>

class QnAbstractDTSFactory;

class QN_EXPORT QnVirtualCameraResource : public QnSecurityCamResource
{
    Q_OBJECT
    Q_FLAGS(Qn::CameraCapabilities)
    Q_PROPERTY(Qn::CameraCapabilities cameraCapabilities READ getCameraCapabilities WRITE setCameraCapabilities)

public:
    QnVirtualCameraResource();

    QnAbstractDTSFactory* getDTSFactory();
    void setDTSFactory(QnAbstractDTSFactory* factory);
    void lockDTSFactory();
    void unLockDTSFactory();

    virtual QString getUniqueId() const override;

    virtual void deserialize(const QnResourceParameters& parameters) override;

    QString toSearchString() const override;

public slots:
    void issueOccured();
    void noCameraIssues();
private slots:
    void at_saveAsyncFinished(int, const QnResourceList &, int);
protected:
    void save();
    int saveAsync(QObject *target, const char *slot);

private:
    QnAbstractDTSFactory* m_dtsFactory;
    std::deque<qint64> m_issueTimes;
};

const QSize EMPTY_RESOLUTION_PAIR(0, 0);
const QSize SECONDARY_STREAM_DEFAULT_RESOLUTION(480, 316); // 316 is average between 272&360
const QSize SECONDARY_STREAM_MAX_RESOLUTION(1024, 768);

class QN_EXPORT QnPhysicalCameraResource : public QnVirtualCameraResource
{
    Q_OBJECT
public:
    QnPhysicalCameraResource();

    // the difference between desired and real is that camera can have multiple clients we do not know about or big exposure time
    int getPrimaryStreamRealFps() const;

    virtual int suggestBitrateKbps(Qn::StreamQuality q, QSize resolution, int fps) const;

    virtual void setUrl(const QString &url) override;
    virtual int getChannel() const override;

    static float getResolutionAspectRatio(const QSize& resolution); // find resolution helper function
    static QSize getNearestResolution(const QSize& resolution, float aspectRatio, double maxResolutionSquare, const QList<QSize>& resolutionList); // find resolution helper function
protected:
    virtual CameraDiagnostics::Result initInternal() override;
private:
    int m_channelNumber; // video/audio source number
};

Q_DECLARE_METATYPE(QnVirtualCameraResourcePtr);
Q_DECLARE_METATYPE(QnVirtualCameraResourceList);

#endif // QN_CAMERA_RESOURCE_H
