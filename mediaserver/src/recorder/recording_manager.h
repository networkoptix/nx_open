#ifndef __RECORDING_MANAGER_H__
#define __RECORDING_MANAGER_H__

#include <QTimer>
#include <QObject>
#include <QMap>
#include "core/resource/resource.h"
#include "core/resource/security_cam_resource.h"
#include "core/misc/schedule_task.h"

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

class QnRecordingManager: public QThread
{
    Q_OBJECT
public:
    static const int RECORDING_CHUNK_LEN = 60; // seconds
    static const int MIN_SECONDARY_FPS = 2;


    static QnRecordingManager* instance();
    QnRecordingManager();
    virtual ~QnRecordingManager();

    void start();
    void stop();
    bool isCameraRecoring(QnResourcePtr camera);

    Recorders findRecorders(QnResourcePtr res) const;

    bool startForcedRecording(QnSecurityCamResourcePtr camRes, QnStreamQuality quality, int fps, int maxDuration);
    bool stopForcedRecording(QnSecurityCamResourcePtr camRes);
private slots:
    void onNewResource(QnResourcePtr res);
    void onRemoveResource(QnResourcePtr res);
    void onTimer();
    void at_updateStorage();
    void at_cameraStatusChanged(QnResource::Status oldStatus, QnResource::Status newStatus);
    void at_cameraUpdated();
    void at_initAsyncFinished(QnResourcePtr res, bool state);
private:
    void updateCamera(QnSecurityCamResourcePtr camera);

    QnServerStreamRecorder* createRecorder(QnResourcePtr res, QnVideoCamera* camera, QnResource::ConnectionRole role);
    void startOrStopRecording(QnResourcePtr res, QnVideoCamera* camera, QnServerStreamRecorder* recorderHiRes, QnServerStreamRecorder* recorderLowRes);
    bool isResourceDisabled(QnResourcePtr res) const;

    void beforeDeleteRecorder(const Recorders& recorders);
    void deleteRecorder(const Recorders& recorders);
    bool updateCameraHistory(QnResourcePtr res);
private:
    mutable QMutex m_mutex;
    QMap<QnResourcePtr, Recorders> m_recordMap;
    QTimer m_scheduleWatchingTimer;
};

class QnServerDataProviderFactory: public QnDataProviderFactory
{
public:
    static QnServerDataProviderFactory* instance();
    QnAbstractStreamDataProvider* createDataProviderInternal(QnResourcePtr res, QnResource::ConnectionRole role);
};

#define qnRecordingManager QnRecordingManager::instance()

#endif // __RECORDING_MANAGER_H__
