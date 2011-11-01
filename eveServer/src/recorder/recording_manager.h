#ifndef __RECORDING_MANAGER_H__
#define __RECORDING_MANAGER_H__

#include <QObject>
#include "core/resource/resource.h"
#include "core/resourcemanagment/security_cam_resource.h"

class QnRecordingManager: public QObject
{
    Q_OBJECT
public:
    QnRecordingManager();
    virtual ~QnRecordingManager();

    void start();
private slots:
    void onNewResource(QnResourcePtr res);
    void onRemoveResource(QnResourcePtr res);
    void recordingFailed(QString errMessage);
private:
    //QnStreamRecorder* m_recorder;
};

class QnServerDataProviderFactory: public QnDataProviderFactory
{
public:
    static QnServerDataProviderFactory* instance();
    QnAbstractStreamDataProvider* createDataProviderInternal(QnResourcePtr res, QnResource::ConnectionRole role);
};

#endif // __RECORDING_MANAGER_H__
