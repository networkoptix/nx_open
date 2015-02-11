#ifndef __RECORDING_MANAGER_H__
#define __RECORDING_MANAGER_H__

#include <QtCore/QTimer>
#include <QtCore/QObject>
#include <QtCore/QMap>

#include "core/resource/resource_fwd.h"
#include "business/business_fwd.h"
#include <server/server_globals.h>

#include <core/resource/resource_fwd.h>

#include <core/dataprovider/data_provider_factory.h>

#include "core/misc/schedule_task.h"

class QnServerStreamRecorder;
class QnVideoCamera;
namespace ec2 {
    class QnDistributedMutex;
}

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
    bool isCameraRecoring(const QnResourcePtr& camera);

    Recorders findRecorders(const QnResourcePtr& res) const;

    bool startForcedRecording(const QnSecurityCamResourcePtr& camRes, Qn::StreamQuality quality, int fps, int beforeThreshold, int afterThreshold, int maxDuration);

    bool stopForcedRecording(const QnSecurityCamResourcePtr& camRes, bool afterThresholdCheck = true);
signals:
    void recordingDisabled(const QnResourcePtr &resource, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString& reasonText);
private slots:
    void onNewResource(const QnResourcePtr &resource);
    void onRemoveResource(const QnResourcePtr &resource);
    void onTimer();
    void at_camera_initializationChanged(const QnResourcePtr &resource);
    void at_camera_resourceChanged(const QnResourcePtr &resource);
    void at_checkLicenses();
    void at_historyMutexLocked();
    void at_historyMutexTimeout();
private:
    void updateCamera(const QnSecurityCamResourcePtr& camera);

    QnServerStreamRecorder* createRecorder(const QnResourcePtr &res, QnVideoCamera* camera, QnServer::ChunksCatalog catalog);
    bool startOrStopRecording(const QnResourcePtr& res, QnVideoCamera* camera, QnServerStreamRecorder* recorderHiRes, QnServerStreamRecorder* recorderLowRes);
    bool isResourceDisabled(const QnResourcePtr& res) const;
    QnVirtualCameraResourceList getLocalControlledCameras() const;

    void beforeDeleteRecorder(const Recorders& recorders);
    void stopRecorder(const Recorders& recorders);
    void deleteRecorder(const Recorders& recorders, const QnResourcePtr& resource);
    void updateCameraHistory(const QnResourcePtr& res);

    void at_licenseMutexLocked();
    void at_licenseMutexTimeout();
private:
    struct LockData 
    {
        LockData(LockData&& other);
        LockData();
        LockData(ec2::QnDistributedMutex* mutex, QnVirtualCameraResourcePtr cameraResource, qint64 currentTime);
        ~LockData();

        ec2::QnDistributedMutex* mutex;
        QnVirtualCameraResourcePtr cameraResource;
        qint64 currentTime;
    private:
        LockData(const LockData& other);
    };
    std::map<QString, LockData> m_lockInProgress;

    mutable QnMutex m_mutex;
    QMap<QnResourcePtr, Recorders> m_recordMap;
    QTimer m_scheduleWatchingTimer;
    QTimer m_licenseTimer;
    QMap<QnSecurityCamResourcePtr, qint64> m_delayedStop;
    ec2::QnDistributedMutex* m_licenseMutex;
    int m_tooManyRecordingCnt;
    qint64 m_recordingStopTime;
};

#define qnRecordingManager QnRecordingManager::instance()

#endif // __RECORDING_MANAGER_H__
