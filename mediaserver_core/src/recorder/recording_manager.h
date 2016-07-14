#ifndef __RECORDING_MANAGER_H__
#define __RECORDING_MANAGER_H__

#include <map>
#include <QtCore/QTimer>
#include <QtCore/QObject>
#include <QtCore/QMap>
#include <QtCore/QElapsedTimer>

#include "core/resource/resource_fwd.h"
#include "business/business_fwd.h"
#include <server/server_globals.h>

#include <core/resource/resource_fwd.h>
#include <core/dataprovider/data_provider_factory.h>

#include "camera/video_camera.h"
#include "core/misc/schedule_task.h"


class QnServerStreamRecorder;
class QnVideoCamera;
class QnDualStreamingHelper;

namespace ec2 {
    class QnDistributedMutex;
}

struct Recorders
{
    Recorders(): recorderHiRes(0), recorderLowRes(0) {}

    QnServerStreamRecorder* recorderHiRes;
    QnServerStreamRecorder* recorderLowRes;
    QSharedPointer<QnDualStreamingHelper> dualStreamingHelper;
};

class WriteBufferMultiplierManager : public QObject
{
    Q_OBJECT
    struct Key
    {
        QnServer::ChunksCatalog catalog;
        QnUuid resourceId;
        Key(QnServer::ChunksCatalog catalog, const QnUuid& resourceId) :
            catalog(catalog),
            resourceId(resourceId)
        {}
        Key() {}
    };
    friend bool operator < (const Key& lhs, const Key& rhs)
    {
        return lhs.resourceId < rhs.resourceId ? 
            true : 
            lhs.catalog < rhs.catalog;
    }
    typedef std::map<Key, int> RecToSizeType;
    typedef std::map<uintptr_t, Key> FileToRecType;

public:
    int getSizeForCam(
        QnServer::ChunksCatalog catalog, 
        const QnUuid& resourceId);
    void setFilePtr(
        uintptr_t filePtr,
        QnServer::ChunksCatalog catalog, 
        const QnUuid& resourceId);
public slots:
    void at_seekDetected(uintptr_t filePtr, int size);
    void at_fileClosed(uintptr_t filePtr);
        
private:
    RecToSizeType m_recToMult;
    FileToRecType m_fileToRec;
    QnMutex m_mutex;
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
    bool isCameraRecoring(const QnResourcePtr& camera) const;

    Recorders findRecorders(const QnResourcePtr& res) const;

    bool startForcedRecording(const QnSecurityCamResourcePtr& camRes, Qn::StreamQuality quality, int fps, int beforeThreshold, int afterThreshold, int maxDuration);

    bool stopForcedRecording(const QnSecurityCamResourcePtr& camRes, bool afterThresholdCheck = true);

    WriteBufferMultiplierManager& getBufferManager() { return m_writeBufferManager; }
signals:
    void recordingDisabled(const QnResourcePtr &resource, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString& reasonText);
private slots:
    void onNewResource(const QnResourcePtr &resource);
    void onRemoveResource(const QnResourcePtr &resource);
    void onTimer();
    void at_camera_initializationChanged(const QnResourcePtr &resource);
    void at_camera_resourceChanged(const QnResourcePtr &resource);
    void at_checkLicenses();
    void at_serverPropertyChanged(const QnResourcePtr &, const QString &key);
private:
    void updateCamera(const QnSecurityCamResourcePtr& camera);

    QnServerStreamRecorder* createRecorder(const QnResourcePtr &res, const QSharedPointer<QnAbstractMediaStreamDataProvider>& reader, 
                                           QnServer::ChunksCatalog catalog, const QSharedPointer<QnDualStreamingHelper>& dualStreamingHelper);
    bool startOrStopRecording(const QnResourcePtr& res, const QnVideoCameraPtr& camera, QnServerStreamRecorder* recorderHiRes, QnServerStreamRecorder* recorderLowRes);
    bool isResourceDisabled(const QnResourcePtr& res) const;
    QnVirtualCameraResourceList getLocalControlledCameras() const;

    void beforeDeleteRecorder(const Recorders& recorders);
    void stopRecorder(const Recorders& recorders);
    void deleteRecorder(const Recorders& recorders, const QnResourcePtr& resource);

    void at_licenseMutexLocked();
    void at_licenseMutexTimeout();

    void updateCameraHistoryNonSafe(
        const QString uniqueCameraId,
        qint64 currentTime);

private:
    mutable QnMutex m_mutex;
    QMap<QnResourcePtr, Recorders> m_recordMap;
    QTimer m_scheduleWatchingTimer;
    QTimer m_licenseTimer;
    QMap<QnSecurityCamResourcePtr, qint64> m_delayedStop;
    ec2::QnDistributedMutex* m_licenseMutex;
    int m_tooManyRecordingCnt;
    qint64 m_recordingStopTime;
    WriteBufferMultiplierManager m_writeBufferManager;
};

#define qnRecordingManager QnRecordingManager::instance()

#endif // __RECORDING_MANAGER_H__
