#ifndef __RECORDING_MANAGER_H__
#define __RECORDING_MANAGER_H__

#include <QtCore/QTimer>
#include <QtCore/QObject>
#include <QtCore/QMap>

#include <server/server_globals.h>

#include "core/resource/security_cam_resource.h"
#include "core/misc/schedule_task.h"
#include "mutex/distributed_mutex.h"
#include "business/business_fwd.h"

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


    static void initStaticInstance( QnRecordingManager* );
    static QnRecordingManager* instance();

    QnRecordingManager();
    virtual ~QnRecordingManager();

    void start();
    void stop();
    bool isCameraRecoring(QnResourcePtr camera);

    Recorders findRecorders(QnResourcePtr res) const;

    bool startForcedRecording(QnSecurityCamResourcePtr camRes, Qn::StreamQuality quality, int fps, int beforeThreshold, int afterThreshold, int maxDuration);

    bool stopForcedRecording(QnSecurityCamResourcePtr camRes, bool afterThresholdCheck = true);
signals:
    void recordingDisabled(const QnResourcePtr &resource, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString& reasonText);
private slots:
    void onNewResource(const QnResourcePtr &resource);
    void onRemoveResource(const QnResourcePtr &resource);
    void onTimer();
    void at_server_resourceChanged(const QnResourcePtr &resource);
    void at_camera_statusChanged(const QnResourcePtr &resource);
    void at_camera_resourceChanged(const QnResourcePtr &resource);
    void at_camera_initAsyncFinished(const QnResourcePtr &resource, bool state);
    void at_checkLicenses();
private:
    void updateCamera(QnSecurityCamResourcePtr camera);

    QnServerStreamRecorder* createRecorder(const QnResourcePtr &res, QnVideoCamera* camera, QnServer::ChunksCatalog catalog);
    bool startOrStopRecording(QnResourcePtr res, QnVideoCamera* camera, QnServerStreamRecorder* recorderHiRes, QnServerStreamRecorder* recorderLowRes);
    bool isResourceDisabled(QnResourcePtr res) const;
    QnResourceList getLocalControlledCameras();

    void beforeDeleteRecorder(const Recorders& recorders);
    void deleteRecorder(const Recorders& recorders);
    bool updateCameraHistory(QnResourcePtr res);

    void at_licenseMutexLocked(QString name);
    void at_licenseMutexTimeout(QString name);
private:
    mutable QMutex m_mutex;
    QSet<QnResourcePtr> m_onlineCameras;
    QMap<QnResourcePtr, Recorders> m_recordMap;
    QTimer m_scheduleWatchingTimer;
    QTimer m_licenseTimer;
    QMap<QnSecurityCamResourcePtr, qint64> m_delayedStop;
    ec2::QnDistributedMutexPtr m_licenseMutex;
    int m_tooManyRecordingCnt;
};

class QnServerDataProviderFactory: public QnDataProviderFactory
{
public:
    static QnServerDataProviderFactory* instance();
    QnAbstractStreamDataProvider* createDataProviderInternal(QnResourcePtr res, QnResource::ConnectionRole role);
};

#define qnRecordingManager QnRecordingManager::instance()

#endif // __RECORDING_MANAGER_H__
