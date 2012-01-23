#ifndef __RECORDING_MANAGER_H__
#define __RECORDING_MANAGER_H__

#include <QObject>
#include <QMap>
#include "core/resource/resource.h"
#include "core/resource/security_cam_resource.h"
#include "core/misc/scheduleTask.h"

class QnServerStreamRecorder;
class QnRecordingManager: public QObject
{
    Q_OBJECT
public:
    static QnRecordingManager* instance();
    QnRecordingManager();
    virtual ~QnRecordingManager();

    void start();
    void stop();
    bool isCameraRecoring(QnResourcePtr camera);

    void updateSchedule(QnSecurityCamResourcePtr camera);

    QnServerStreamRecorder* findRecorder(QnResourcePtr res) const;
private slots:
    void onNewResource(QnResourcePtr res);
    void onRemoveResource(QnResourcePtr res);
    void recordingFailed(QString errMessage);
private:
    mutable QMutex m_mutex;

    QMap<QnResourcePtr, QnServerStreamRecorder*> m_recordMap;
    QMap<QnId, QnScheduleTaskList> m_scheduleByCamera;
};

class QnServerDataProviderFactory: public QnDataProviderFactory
{
public:
    static QnServerDataProviderFactory* instance();
    QnAbstractStreamDataProvider* createDataProviderInternal(QnResourcePtr res, QnResource::ConnectionRole role);
};

#endif // __RECORDING_MANAGER_H__
