#ifndef __RECORDING_MANAGER_H__
#define __RECORDING_MANAGER_H__

#include <QObject>
#include <QMap>
#include "core/resource/resource.h"
#include "core/resourcemanagment/security_cam_resource.h"

class QnStreamRecorder;
class QnRecordingManager: public QObject
{
    Q_OBJECT
public:
    static QnRecordingManager* instance();
    QnRecordingManager();
    virtual ~QnRecordingManager();

    void start();
    bool isCameraRecoring(QnResourcePtr camera);
private slots:
    void onNewResource(QnResourcePtr res);
    void onRemoveResource(QnResourcePtr res);
    void recordingFailed(QString errMessage);
private:
    QMap<QnResourcePtr, QnStreamRecorder*> m_recordMap;
};

class QnServerDataProviderFactory: public QnDataProviderFactory
{
public:
    static QnServerDataProviderFactory* instance();
    QnAbstractStreamDataProvider* createDataProviderInternal(QnResourcePtr res, QnResource::ConnectionRole role);
};

#endif // __RECORDING_MANAGER_H__
