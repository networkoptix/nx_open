#ifndef __RECORDING_MANAGER_H__
#define __RECORDING_MANAGER_H__

#include <QObject>
#include <QMap>
#include "core/resource/resource.h"
#include "core/resource/security_cam_resource.h"
#include "core/misc/scheduleTask.h"

class QnServerStreamRecorder;
class QnVideoCamera;

struct Recorders
{
    Recorders(): recorderHiRes(0), recorderLowRes(0) {}
    Recorders(QnServerStreamRecorder* _recorderHiRes, QnServerStreamRecorder* _recorderLowRes):
    recorderHiRes(_recorderHiRes), recorderLowRes(_recorderLowRes) {}
    QnServerStreamRecorder* recorderHiRes;
    QnServerStreamRecorder* recorderLowRes;
};

class QnRecordingManager: public QObject
{
    Q_OBJECT
public:
    static const int RECORDING_CHUNK_LEN = 60; // seconds


    static QnRecordingManager* instance();
    QnRecordingManager();
    virtual ~QnRecordingManager();

    void start();
    void stop();
    bool isCameraRecoring(QnResourcePtr camera);

    void updateCamera(QnSecurityCamResourcePtr camera);

    Recorders findRecorders(QnResourcePtr res) const;
private slots:
    void onNewResource(QnResourcePtr res);
    void onRemoveResource(QnResourcePtr res);
    void onFpsChanged(float value);
    void onResourceStatusChanged(QnResource::Status oldStatus, QnResource::Status newStatus);
private:
    QnServerStreamRecorder* createRecorder(QnResourcePtr res, QnVideoCamera* camera, QnResource::ConnectionRole role);
private:
    mutable QMutex m_mutex;
    QMap<QnResourcePtr, Recorders> m_recordMap;
};

class QnServerDataProviderFactory: public QnDataProviderFactory
{
public:
    static QnServerDataProviderFactory* instance();
    QnAbstractStreamDataProvider* createDataProviderInternal(QnResourcePtr res, QnResource::ConnectionRole role);
};

#endif // __RECORDING_MANAGER_H__
